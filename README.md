# RedundantScopeChecker
Clang plugin to warn about global variables that can be local.

This was compiler design course project.

The problem statement was to warn about global variables which are effectively used in a single scope.

Additional features implemented:

* Skip special cases (extern variables, variables with non trivial initializer, global constant used to initialize another global).

* `__attribute__((rcs_ignore))` to annotate variables to be excluded from this warning. `__attribute__((used))` can also be used instead.

* Show usages as 'note' diagnostics under warnings.

* Some more features selected through command line options.

