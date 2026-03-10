#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "lexer.h"

Token tokens[MAX_TOKENS];
int nTokens;

int line = 1; // the current line in the input file

// adds a token to the end of the tokens list and returns it
// sets its code and line
Token *addTk(int code) {
    if (nTokens == MAX_TOKENS)err("too many tokens");
    Token *tk = &tokens[nTokens];
    tk->code = code;
    tk->line = line;
    nTokens++;
    return tk;
}

// copy in the dst buffer the string between [begin,end)
char *copyn(char *dst, const char *begin, const char *end) {
    char *p = dst;
    if (end - begin > MAX_STR)err("string too long");
    while (begin != end)*p++ = *begin++;
    *p = '\0';
    return dst;
}

void tokenize(const char *pch) {
    const char *start;
    Token *tk;
    for (;;) {
        char buf[MAX_STR + 1];
        switch (*pch) {
            case ' ':
            case '\t': pch++;
                break;
            case '\r': // handles different kinds of newlines (Windows: \r\n, Linux: \n, MacOS, OS X: \r or \n)
                if (pch[1] == '\n')pch++;
            // fallthrough to \n
            case '\n':
                line++;
                pch++;
                break;
            case ',': addTk(COMMA);
                pch++;
                break;
            case ':': addTk(COLON);
                pch++;
                break;
            case ';': addTk(SEMICOLON);
                pch++;
                break;
            case '(': addTk(LPAR);
                pch++;
                break;
            case ')': addTk(RPAR);
                pch++;
                break;
            case '\0': addTk(FINISH);
                return;
            case '+': addTk(ADD);
                pch++;
                break;
            case '-': addTk(SUB);
                pch++;
                break;
            case '*': addTk(MUL);
                pch++;
                break;
            case '/':
                if (pch[1] == '/') {
                    pch += 2;
                    while (*pch != '\n' && *pch != '\r' && *pch != '\0') pch++;
                } else {
                    addTk(DIV);
                    pch++;
                }
                break;
            case '&':
                if (pch[1] == '&') {
                    addTk(AND);
                    pch += 2;
                } else err("invalid char at line %d, expected \"&\": %c (%d)", line, *pch, *pch);
                break;
            case '|':
                if (pch[1] == '|') {
                    addTk(OR);
                    pch += 2;
                } else err("invalid char at line %d, expected \"|\": %c (%d)", line, *pch, *pch);
                break;
            case '!':
                if (pch[1] == '=') {
                    addTk(NOTEQ);
                    pch += 2;
                } else {
                    addTk(NOT);
                    pch++;
                }
                break;
            case '=':
                if (pch[1] == '=') {
                    addTk(EQUAL);
                    pch += 2;
                } else {
                    addTk(ASSIGN);
                    pch++;
                }
                break;
            case '<':
                if (pch[1] == '=') {
                    addTk(LESSEQ);
                    pch += 2;
                } else {
                    addTk(LESS);
                    pch++;
                }
                break;
            case '>':
                if (pch[1] == '=') {
                    addTk(GREATEREQ);
                    pch += 2;
                } else {
                    addTk(GREATER);
                    pch++;
                }
                break;
            default:
                if (isalpha(*pch) || *pch == '_') {
                    for (start = pch++; isalnum(*pch) || *pch == '_'; pch++) {
                    }
                    char *text = copyn(buf, start, pch);
                    if (strcmp(text, "int") == 0)addTk(TYPE_INT);
                    else if (strcmp(text, "float") == 0)addTk(TYPE_REAL);
                    else if (strcmp(text, "str") == 0)addTk(TYPE_STR);
                    else if (strcmp(text, "return") == 0)addTk(RETURN);
                    else if (strcmp(text, "end") == 0)addTk(END);
                    else if (strcmp(text, "if") == 0)addTk(IF);
                    else if (strcmp(text, "else") == 0)addTk(ELSE);
                    else if (strcmp(text, "while") == 0)addTk(WHILE);
                    else if (strcmp(text, "var") == 0)addTk(VAR);
                    else if (strcmp(text, "function") == 0)addTk(FUNCTION);
                    else {
                        tk = addTk(ID);
                        strcpy(tk->text, text);
                    }
                } else if (isdigit(*pch)) {
                    start = pch;
                    while (isdigit(*pch))
                        pch++;
                    if (*pch == '.') {
                        if (!isdigit(pch[1]))
                            err("invalid char at line %d, expected digit after \".\" (%d)", line, *pch);
                        pch++;
                        while (isdigit(*pch)) pch++;
                        char *text = copyn(buf, start, pch);
                        tk = addTk(REAL);
                        tk->r = atof(text);
                    } else {
                        char *text = copyn(buf, start, pch);
                        tk = addTk(INT);
                        tk->i = atoi(text);
                    }
                } else if (*pch == '"') {
                    pch++;
                    start = pch;
                    while (*pch != '"' && *pch != '\n' && *pch != '\0')
                        pch++;
                    if (*pch != '"') err("unterminated string literal at line %d", line);
                    char *text = copyn(buf, start, pch);
                    tk = addTk(STR);
                    strcpy(tk->text, text);
                    pch++;
                } else err("invalid char at line %d: %c (%d)", line, *pch, *pch);
        }
    }
}

void showTokens() {
    for (int i = 0; i < nTokens; i++) {
        Token *tk = &tokens[i];

        printf("%d ", tk->line);

        switch (tk->code) {
            case VAR:
            case FUNCTION:
            case IF:
            case ELSE:
            case WHILE:
            case END:
            case RETURN:
                printf("%s\n", (tk->code == VAR)
                                   ? "VAR"
                                   : (tk->code == FUNCTION)
                                         ? "FUNCTION"
                                         : (tk->code == IF)
                                               ? "IF"
                                               : (tk->code == ELSE)
                                                     ? "ELSE"
                                                     : (tk->code == WHILE)
                                                           ? "WHILE"
                                                           : (tk->code == END)
                                                                 ? "END"
                                                                 : (tk->code == RETURN)
                                                                       ? "RETURN"
                                                                       : "");
                break;
            case TYPE_INT:
                printf("TYPE_INT\n");
                break;
            case TYPE_REAL:
                printf("TYPE_REAL\n");
                break;
            case TYPE_STR:
                printf("TYPE_STR\n");
                break;
            case ID:
                printf("ID:%s\n", tk->text);
                break;
            case INT:
                printf("INT:%d\n", tk->i);
                break;
            case REAL:
                printf("REAL:%f\n", tk->r);
                break;
            case STR:
                printf("STR:%s\n", tk->text);
                break;
            case COMMA:
                printf("COMMA\n");
                break;
            case COLON:
                printf("COLON\n");
                break;
            case SEMICOLON:
                printf("SEMICOLON\n");
                break;
            case LPAR:
                printf("LPAR\n");
                break;
            case RPAR:
                printf("RPAR\n");
                break;
            case ASSIGN:
                printf("ASSIGN\n");
                break;
            case EQUAL:
                printf("EQUAL\n");
                break;
            case NOTEQ:
                printf("NOTEQ\n");
                break;
            case LESS:
                printf("LESS\n");
                break;
            case GREATER:
                printf("GREATER\n");
                break;
            case GREATEREQ:
                printf("GREATEREQ\n");
                break;
            case ADD:
                printf("ADD\n");
                break;
            case SUB:
                printf("SUB\n");
                break;
            case MUL:
                printf("MUL\n");
                break;
            case DIV:
                printf("DIV\n");
                break;
            case AND:
                printf("AND\n");
                break;
            case OR:
                printf("OR\n");
                break;
            case NOT:
                printf("NOT\n");
                break;
            case FINISH:
                printf("FINISH\n");
                break;
            default:
                printf("Unknown token type\n");
        }
    }
}
