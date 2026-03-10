#include <stdlib.h>
#include "lexer.h"
#include "utils.h"
#include "parser.h"

int main() {
    char *src = loadFile("1.q");
    tokenize(src);
    //showTokens();
    parse();
    free(src);
    return 0;
}
