// simple example of a clang plugin tool
// which issues warning diagnostics on
// finding variables, functions and class names beginning with _

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/FileManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/raw_ostream.h"
using namespace clang;

struct UsageInformation {
	Stmt *usedIn;
	CompoundStmt *parent;
	std::vector<UsageInformation> children;
};

/// Plugin options
struct {
	// dump AST of top level components (not in header)
	bool dumpAst = false;

	// Do not warn on globals initialized with constructor calls
	// Note - warning will be still generated if constructor is constexpr
	bool noWarnCallInit = false;

	// do not warn on unused variables
	bool noWarnUnused = false;

	// do not show usage locations, only print outermost scope
	bool noShowUsages = false;

	// verbose (for debugging)
	bool verbose = false;
} options;

void fatal(const std::string &message) {
	llvm::errs() << message << "\n";
	exit(1);
}

void parseArgs(const std::vector<std::string> &args) {
	std::unordered_map<std::string, bool *> valid = {
	    {"-dump-ast", &options.dumpAst},
	    {"-no-warn-call-init", &options.noWarnCallInit},
	    {"-no-warn-unused", &options.noWarnUnused},
	    {"-no-show-usages", &options.noShowUsages},
	    {"-verbose", &options.verbose},
	};
	for (auto &s : args) {
		if (valid.count(s)) {
			if (!*valid[s]) {
				*valid[s] = true;
			} else {
				fatal("same option specified twice: " + s);
			}
		} else {
			fatal("unknown option: " + s);
		}
	}
}

// For debugging
template <typename V, typename... T> void verbose(V v, T... t) {
	llvm::errs() << v;
	verbose(t...);
}

void verbose() { llvm::errs() << "\n"; }

class ScopeCheckerVisitor : public RecursiveASTVisitor<ScopeCheckerVisitor> {
      private:
	bool dumpAst;
	ASTContext *context;
	CompilerInstance &instance;
	CompoundStmt *parentStmt = nullptr;

	DiagnosticsEngine &d;

	std::unordered_map<VarDecl *, std::vector<UsageInformation>> usages;

	// merges all children of `compound` in vector under `compound`
	void merge(std::vector<UsageInformation> &v, CompoundStmt *compound,
	           CompoundStmt *parent) {
		std::vector<UsageInformation>::iterator itr;
		for (itr = v.begin(); itr != v.end(); itr++) {
			if (itr->parent == compound)
				break;
		}
		if (itr == v.end()) {
			return;
		}
		// merge [itr, end) into one UsageInformation
		*itr = (UsageInformation){
		    compound, parent,
		    std::vector<UsageInformation>(itr, v.end())};
		v.erase(itr + 1, v.end());
	}

	int depth = 0;
	unsigned int unusedWarning, redundantScopeWarning, usageNote,
	    usageStmtNote;

	bool isInHeader(Decl *decl) {
		auto loc = decl->getLocation();
		auto floc = context->getFullLoc(loc);
		if (floc.isInSystemHeader())
			return true;
		auto file = floc.getFileEntry();
		if (file == nullptr)
			return true;
		auto entry = file->getName();
		if (entry.endswith(".cpp") || entry.endswith(".cc") ||
		    entry.endswith(".c")) {
			return false;
		}
		return true;
	}

      public:
	void mergeAll(CompoundStmt *stmt, CompoundStmt *parent) {
		for (auto &entry : usages) {
			auto &uses = entry.second;
			if (uses.empty()) {
				continue;
			}
			merge(uses, stmt, parent);
		}
	}

	bool isRcsIgnore(VarDecl *decl) {
		auto attrs = decl->getAttrs();
		for (auto attr : attrs) {
			if (strcmp(attr->getSpelling(), "annotate") == 0 &&
			    ((AnnotateAttr *)attr)->getAnnotation() ==
			        "rcs_ignore") {
				return true;
			}
		}
		return false;
	}

	bool hasCallInit(VarDecl *decl) {
		if (decl->getInitStyle() !=
		    VarDecl::InitializationStyle::CallInit) {
			return false;
		}
		auto init = decl->getInit();
		auto langOpts = context->getLangOpts();
		if (init->isCXX11ConstantExpr(*context) ||
		    init->isConstantInitializer(*context, false)) {
			return false;
		}
		return true;
	}

	void printRedundant() {
		for (auto &entry : usages) {
			auto vdecl = entry.first;
			if (isRcsIgnore(vdecl)) {
				continue;
			}
			if (options.noWarnCallInit && hasCallInit(vdecl)) {
				continue;
			}
			auto &uses = entry.second;

			// used in multiple places
			if (uses.size() > 1) {
				continue;
			}

			if (vdecl->hasExternalStorage()) {
				continue;
			}

			auto loc = context->getFullLoc(vdecl->getLocation());
			if (uses.empty()) {
				if (!options.noWarnUnused) {
					d.Report(loc, unusedWarning)
					    << vdecl->getNameAsString();
				}
				continue;
			}

			auto outest = uses[0].usedIn;
			if (uses[0].children.empty()) {
				// single use in global scope
				continue;
			}

			if (outest != nullptr) {
				d.Report(loc, redundantScopeWarning)
				    << vdecl->getNameAsString();
				if (!options.noShowUsages) {
					printNotes(vdecl, uses);
				}
			}
		}
	}

	void printNotes(VarDecl *vdecl, std::vector<UsageInformation> &uses) {
		for (auto &use : uses) {
			if (use.children.empty()) {
				auto loc = context->getFullLoc(
				    (use.usedIn)->getBeginLoc());
				d.Report(loc, usageStmtNote);
			} else {
				auto loc = context->getFullLoc(
				    (use.usedIn)->getBeginLoc());
				d.Report(loc, usageNote);
				printNotes(vdecl, use.children);
			}
		}
	}

	explicit ScopeCheckerVisitor(ASTContext *context,
	                             CompilerInstance &instance, bool dumpAst)
	    : context(context), instance(instance),
	      d(instance.getDiagnostics()), dumpAst(dumpAst) {
		unusedWarning = d.getCustomDiagID(DiagnosticsEngine::Warning,
		                                  "Unused variable: '%0'");
		redundantScopeWarning =
		    d.getCustomDiagID(DiagnosticsEngine::Warning,
		                      "variable %0 only used in a smaller "
		                      "scope, consider moving it.");
		usageNote = d.getCustomDiagID(
		    DiagnosticsEngine::Note, ":::::::: In this block ::::::::");
		usageStmtNote =
		    d.getCustomDiagID(DiagnosticsEngine::Note, "Used here.");
	}

	bool VisitDeclRefExpr(DeclRefExpr *e) {
		if (const auto decl = e->getFoundDecl()) {
			if (isInHeader(decl)) {
				return true;
			}
			if (decl->getKind() == Decl::Kind::Var) {
				VarDecl *vd = dynamic_cast<VarDecl *>(decl)
						  ->getCanonicalDecl();
				if (usages.count(vd)) {
					// add the current compound statement to
					// usages vector
					usages[vd].push_back((UsageInformation){
					    e, parentStmt, {}});
				}
			}
		}
		return true;
	}

	bool VisitVarDecl(VarDecl *decl) {
		// Ignore variables defined in headers
		if (isInHeader(decl)) {
			return true;
		}
		// ignore function parameters
		if (auto const pd = dynamic_cast<ParmVarDecl *>(decl)) {
			return true;
		}
		if (depth == 0) {
			auto cd = decl->getCanonicalDecl();
			usages[cd] = {};
		}
		return true;
	}

	// a scope generally begins with a compound statement
	// We need to produce a warning with few notes
	// Location of declaration, and the first compound statement
	// in which the declaration is used.
	//
	// When we traverse down, we keep a stack of compound statements,
	// so we can backtrack to parent scopes easily.
	// We associate few things with each declaration.

	// Override (actually wrap) TraverseCompoundStatement to
	// keep track of scopes.
	bool TraverseCompoundStmt(CompoundStmt *stmt) {
		depth++;
		// implicit stack
		auto parent = parentStmt;
		parentStmt = stmt;
		auto result =
		    static_cast<RecursiveASTVisitor<ScopeCheckerVisitor> *>(
			this)
			->TraverseCompoundStmt(stmt);
		mergeAll(stmt, parent);
		parentStmt = parent;
		depth--;
		return result;
	}
};

class ScopeCheckerConsumer : public ASTConsumer {
	CompilerInstance &instance;
	ScopeCheckerVisitor visitor;

      public:
	ScopeCheckerConsumer(CompilerInstance &instance, bool dumpAst)
	    : instance(instance),
	      visitor(&instance.getASTContext(), instance, dumpAst) {}

	virtual void HandleTranslationUnit(ASTContext &context) override {
		visitor.TraverseDecl(context.getTranslationUnitDecl());
		visitor.printRedundant();
	}
};

class ScopeCheckerAction : public PluginASTAction {
      private:
	bool dumpAst;

      protected:
	virtual std::unique_ptr<ASTConsumer>
	CreateASTConsumer(CompilerInstance &instance,
	                  llvm::StringRef) override {
		return std::make_unique<ScopeCheckerConsumer>(instance,
		                                              dumpAst);
	}

	virtual bool ParseArgs(const CompilerInstance &,
	                       const std::vector<std::string> &opts) override {
		parseArgs(opts);
		return true;
	}

	virtual PluginASTAction::ActionType getActionType() override {
		return PluginASTAction::AddBeforeMainAction;
	}
};

static FrontendPluginRegistry::Add<ScopeCheckerAction> ScopeChecker(
    "RedundantScopeChecker",
    "Warn against redundantly global-scoped variable declarations.");

// Register rcs_ignore attribute
class RcsIgnoreAttr : public ParsedAttrInfo {
      public:
	RcsIgnoreAttr() {
		static constexpr Spelling s[] = {
		    {ParsedAttr::AS_CXX11, "rcs_ignore"},
		    {ParsedAttr::AS_C2x, "rcs_ignore"},
		    {ParsedAttr::AS_GNU, "rcs_ignore"}};
		Spellings = s;
	}

	AttrHandling handleDeclAttribute(Sema &S, Decl *D,
	                                 const ParsedAttr &A) const override {

		D->getCanonicalDecl()->addAttr(
		    AnnotateAttr::Create(S.Context, "rcs_ignore"));
		return AttributeApplied;
	}
};

static ParsedAttrInfoRegistry::Add<RcsIgnoreAttr>
    RcsIgnore("rcs_ignore", "example attribute description");
