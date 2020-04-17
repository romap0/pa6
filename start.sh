export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:`pwd`";
export LD_PRELOAD=`pwd`/libruntime.so;

# clang -std=c99 -Wall -pedantic -o pa2 -g *.c libruntime.dylib
# gcc -std=c99 -Wall -Werror -pedantic -o pa2 -g *.c -L. -lruntime && ./pa2 $* && rm -rf pa2 pa2.dSYM
gcc -std=c99 -Wall -Werror -pedantic -o pa2 -g *.c -L. -lruntime && ./pa2 -p 3 10 20 30 && rm -rf pa2 pa2.dSYM
