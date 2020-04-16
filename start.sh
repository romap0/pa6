export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:`pwd`";

# clang -std=c99 -Wall -pedantic -o pa2 -g *.c libruntime.dylib
gcc -std=c99 -Wall -pedantic -o pa2 -g *.c -L. -lruntime

LD_PRELOAD=`pwd`/libruntime.so ./pa2 $* && rm -rf pa2 pa2.dSYM
