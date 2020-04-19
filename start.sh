export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:`pwd`";
export LD_PRELOAD=`pwd`/libruntime.so;

# clang -std=c99 -Wall -pedantic -o pa4 -g *.c libruntime.dylib
# gcc -std=c99 -Wall -Werror -pedantic -o pa4 -g *.c -L. -lruntime && ./pa4 $* && rm -rf pa4 pa4.dSYM
gcc -std=c99 -Wall -Werror -pedantic -o pa4 -g *.c -L. -lruntime && ./pa4 -p 3 10 20 30 --mutexl && rm -rf pa4 pa4.dSYM
