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
#include "llvm/Support/raw_ostream.h"
using namespace clang;

struct UsageInformation {
	Stmt *usedIn;
	CompoundStmt *parent;
	std::vector<UsageInformation> children;
};

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
		auto entry = floc.getFileEntry()->getName();
		if (entry.endswith(".h") || entry.endswith(".hpp")) {
			return true;
		}
		return false;
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
	void printRedundant() {
		for (auto &entry : usages) {
			auto vdecl = entry.first;
			auto &uses = entry.second;
			if (uses.size() > 1) {
				continue;
			}
			auto loc = context->getFullLoc(vdecl->getLocation());
			if (uses.empty()) {
				d.Report(loc, unusedWarning)
				    << vdecl->getNameAsString();
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
				printNotes(vdecl, uses);
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
		usageNote =
		    d.getCustomDiagID(DiagnosticsEngine::Note,
		                      ":::::::: In this block ::::::::");
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
		if (isInHeader(decl)) {
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
		if (opts.size() != 0 && opts[0] == "-dump-ast") {
			dumpAst = true;
		}
		return true;
	}

	virtual PluginASTAction::ActionType getActionType() override {
		return PluginASTAction::AddAfterMainAction;
	}
};

static FrontendPluginRegistry::Add<ScopeCheckerAction> NoUnderscore(
    "RedundantScopeChecker",
    "Warn against redundantly global-scoped variable declarations.");
