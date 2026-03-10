// This header is not part of the compiler so it will not be added to the project!

// This header implements the library of the functions predefined in Quick
// and it is included from the generated code (from 1.c)

#include <stdio.h>

// defines the data type for TYPE_STR
typedef char *str;

// static - when used for a function, makes it to be private and local in a code file
// so the definition can be duplicated in multiple code files.
// In this way, "quick.h" can be included in multiple code files
// without errors of symbol redefinition (ex: for "puti").
static void puti(int val){
    printf("%d\n",val);
}

static void putr(double val){
    printf("%g\n",val);
}

// "puts" is already declared in stdio.h

