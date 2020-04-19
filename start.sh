export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:`pwd`";
export LD_PRELOAD=`pwd`/libruntime.so;

# clang -std=c99 -Wall -pedantic -o pa6 -g *.c libruntime.dylib
# gcc -std=c99 -Wall -Werror -pedantic -o pa6 -g *.c -L. -lruntime && ./pa6 $* && rm -rf pa6 pa6.dSYM
gcc -std=c99 -Wall -Werror -pedantic -o pa6 -g *.c -L. -lruntime && ./pa6 -p 3 10 20 30 --mutexl && rm -rf pa6 pa6.dSYM
