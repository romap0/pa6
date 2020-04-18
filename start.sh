export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:`pwd`";
export LD_PRELOAD=`pwd`/libruntime.so;

# clang -std=c99 -Wall -pedantic -o pa3 -g *.c libruntime.dylib
# gcc -std=c99 -Wall -Werror -pedantic -o pa3 -g *.c -L. -lruntime && ./pa3 $* && rm -rf pa3 pa3.dSYM
gcc -std=c99 -Wall -Werror -pedantic -o pa3 -g *.c -L. -lruntime && ./pa3 -p 3 10 20 30 && rm -rf pa3 pa3.dSYM
