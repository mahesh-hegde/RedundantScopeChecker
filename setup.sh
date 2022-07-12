mkdir -p plugin
cd plugin
cmake .. && cmake --build .
cd ..
# export LD_LIBRARY_PATH so that we can call clang from anywhere
export LD_LIBRARY_PATH=$PWD/plugin
plugin="-fsyntax-only -fplugin=RedundantScopeChecker.so"
rcs_opt="-Xclang -plugin-arg-RedundantScopeChecker -Xclang"

