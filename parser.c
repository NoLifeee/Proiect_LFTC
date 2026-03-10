#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "lexer.h"
#include "ad.h"
#include "at.h"
#include "gen.h"

int iTk; // the iterator in tokens
Token *consumed; // the last consumed token

// same as err, but also prints the line of the current token
_Noreturn void tkerr(const char *fmt, ...) {
    fprintf(stderr, "error in line %d: ", tokens[iTk].line);
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

bool consume(int code) {
    if (tokens[iTk].code == code) {
        consumed = &tokens[iTk++];
        return true;
    }
    return false;
}

// baseType ::= TYPE_INT | TYPE_REAL | TYPE_STR
bool baseType() {
    if (consume(TYPE_INT)) {
        setRet(TYPE_INT,false);
        return true;
    }
    if (consume(TYPE_REAL)) {
        setRet(TYPE_REAL,false);
        return true;
    }
    if (consume(TYPE_STR)) {
        setRet(TYPE_STR,false);
        return true;
    }
    return false;
}

// defVar ::= VAR ID COLON baseType SEMICOLON
bool defVar() {
    int start = iTk;
    if (consume(VAR)) {
        if (consume(ID)) {
            const char *name = consumed->text;
            Symbol *s = searchInCurrentDomain(name);
            if (s)tkerr("symbol redefinition: %s", name);
            s = addSymbol(name, KIND_VAR);
            s->local = crtFn != NULL;
            if (consume(COLON)) {
                if (baseType()) {
                    s->type = ret.type;
                    if (consume(SEMICOLON)) {
                        Text_write(crtVar,"%s %s;\n",cType(ret.type),name);
                        return true;
                    }
                    tkerr("missing semicolon after variable declaration");
                }
                tkerr("missing type after ':'");
            }
            tkerr("missing ':' after variable name");
        }
        tkerr("missing variable name after 'var'");
    }
    iTk = start;
    return false;
}

// funcParam ::= ID COLON baseType
bool funcParam() {
    int start = iTk;
    if (consume(ID)) {
        const char *name = consumed->text;
        Symbol *s = searchInCurrentDomain(name);
        if (s)tkerr("symbol redefinition: %s", name);
        s = addSymbol(name, KIND_ARG);
        Symbol *sFnParam = addFnArg(crtFn, name);
        if (consume(COLON)) {
            if (baseType()) {
                s->type = ret.type;
                sFnParam->type = ret.type;
                Text_write(&tFnHeader,"%s %s",cType(ret.type),name);
                return true;
            }
            tkerr("missing type after ':'");
        }
        tkerr("missing ':' after parameter name");
    }
    iTk = start;
    return false;
}

// funcParams ::= funcParam ( COMMA funcParam )*
bool funcParams() {
    int start = iTk;
    if (funcParam()) {
        while (consume(COMMA)) {
            Text_write(&tFnHeader,",");
            if (funcParam()) {
            } else tkerr("missing parameter after ','");
        }
        return true;
    }
    iTk = start;
    return false;
}

// Forward declarations
bool expr();

bool block();

// instr ::= expr? SEMICOLON | IF LPAR expr RPAR block ( ELSE block )? END | RETURN expr SEMICOLON | WHILE LPAR expr RPAR block END
bool instr() {
    int start = iTk;

    // IF LPAR expr RPAR block ( ELSE block )? END
    if (consume(IF)) {
        if (consume(LPAR)) {
            Text_write(crtCode,"if(");
            if (expr()) {
                if (ret.type == TYPE_STR)tkerr("the if condition must have type int or real");
                if (consume(RPAR)) {
                    Text_write(crtCode,"){\n");
                    if (block()) {
                        Text_write(crtCode,"}\n");
                        if (consume(ELSE)) {
                            Text_write(crtCode,"else{\n");
                            if (block()) {
                                Text_write(crtCode,"}\n");
                            } else tkerr("missing block after 'else'");
                        }
                        if (consume(END)) {
                            return true;
                        }
                        tkerr("missing 'end' after 'if'");
                    }
                    tkerr("missing block after 'if' condition");
                }
                tkerr("missing ')' after condition");
            }
            tkerr("missing condition in 'if'");
        }
        tkerr("missing '(' after 'if'");
    }

    // RETURN expr SEMICOLON
    if (consume(RETURN)) {
        Text_write(crtCode,"return ");
        if (expr()) {
            if (!crtFn)tkerr("return can be used only in a function");
            if (ret.type != crtFn->type)tkerr("the return type must be the same as the function return type");
            if (consume(SEMICOLON)) {
                Text_write(crtCode,";\n");
                return true;
            }
            tkerr("missing ';' after 'return'");
        }
        tkerr("missing expression after 'return'");
    }

    // WHILE LPAR expr RPAR block END
    if (consume(WHILE)) {
         Text_write(crtCode,"while(");
        if (consume(LPAR)) {
            if (expr()) {
                if(ret.type==TYPE_STR)tkerr("the while condition must have type int or real");
                if (consume(RPAR)) {
                    Text_write(crtCode,"){\n");
                    if (block()) {
                        if (consume(END)) {
                            Text_write(crtCode,"}\n");
                            return true;
                        }
                        tkerr("missing 'end' after 'while'");
                    }
                    tkerr("missing block after 'while' condition");
                }
                tkerr("missing ')' after condition");
            }
            tkerr("missing condition in 'while'");
        }
        tkerr("missing '(' after 'while'");
    }

    // expr? SEMICOLON
    expr(); // optional, so we don't check result
    if (consume(SEMICOLON)) {
        Text_write(crtCode,";\n");
        return true;
    }

    iTk = start;
    return false;
}

// block ::= instr+
bool block() {
    if (instr()) {
        while (instr()) {
        }
        return true;
    }
    return false;
}

// defFunc ::= FUNCTION ID LPAR funcParams? RPAR COLON baseType defVar* block END
bool defFunc() {
    int start = iTk;
    if (consume(FUNCTION)) {
        if (consume(ID)) {
            const char *name = consumed->text;
            Symbol *s = searchInCurrentDomain(name);
            if (s)tkerr("symbol redefinition: %s", name);
            crtFn = addSymbol(name, KIND_FN);
            crtFn->args = NULL;
            addDomain();
            crtCode=&tFunctions;
            crtVar=&tFunctions;
            Text_clear(&tFnHeader);
            Text_write(&tFnHeader,"%s(",name);
            if (consume(LPAR)) {
                if (funcParams()) {
                }
                if (consume(RPAR)) {
                    if (consume(COLON)) {
                        if (baseType()) {
                            crtFn->type = ret.type;
                            Text_write(&tFunctions,"\n%s %s){\n",cType(ret.type),tFnHeader.buf);
                            while (defVar()) {
                            }
                            if (block()) {
                                if (consume(END)) {
                                    delDomain();
                                    crtFn = NULL;
                                    Text_write(&tFunctions,"}\n");
                                    crtCode=&tMain;
                                    crtVar=&tBegin;
                                    return true;
                                }
                                tkerr("missing 'end' after function");
                            }
                            tkerr("missing function body");
                        }
                        tkerr("missing return type after ':'");
                    }
                    tkerr("missing ':' after ')'");
                }
                tkerr("missing ')' after function parameters");
            }
            tkerr("missing '(' after function name");
        }
        tkerr("missing function name after 'function'");
    }
    iTk = start;
    return false;
}

// Forward declaration
bool exprPrefix();

// exprMul ::= exprPrefix ( ( MUL | DIV ) exprPrefix )*
bool exprMul() {
    if (exprPrefix()) {
        for (;;) {
            if (consume(MUL)) {
                Ret leftType=ret;
                if(leftType.type==TYPE_STR)tkerr("the operands of * or / cannot be of type str");
                Text_write(crtCode,"*");
                if (exprPrefix()) {
                    if(leftType.type!=ret.type)tkerr("different types for the operands of * or /");
                    ret.lval=false;
                } else tkerr("missing expression after '*'");
            } else if (consume(DIV)) {
                Ret leftType=ret;
                if(leftType.type==TYPE_STR)tkerr("the operands of * or / cannot be of type str");
                Text_write(crtCode,"/");
                if (exprPrefix()) {
                    if(leftType.type!=ret.type)tkerr("different types for the operands of * or /");
                    ret.lval=false;
                } else tkerr("missing expression after '/'");
            } else break;
        }
        return true;
    }
    return false;
}

// exprAdd ::= exprMul ( ( ADD | SUB ) exprMul )*
bool exprAdd() {
    if (exprMul()) {
        for (;;) {
            if (consume(ADD)) {
                Ret leftType=ret;
                if(leftType.type==TYPE_STR)tkerr("the operands of + or - cannot be of type str");
                Text_write(crtCode,"+");
                if (exprMul()) {
                    if(leftType.type!=ret.type)tkerr("different types for the operands of + or -");
                    ret.lval=false;
                } else tkerr("missing expression after '+'");
            } else if (consume(SUB)) {
                Ret leftType=ret;
                if(leftType.type==TYPE_STR)tkerr("the operands of + or - cannot be of type str");
                Text_write(crtCode,"-");
                if (exprMul()) {
                    if(leftType.type!=ret.type)tkerr("different types for the operands of + or -");
                    ret.lval=false;
                } else tkerr("missing expression after '-'");
            } else break;
        }
        return true;
    }
    return false;
}

// exprComp ::= exprAdd ( ( LESS | EQUAL ) exprAdd )?
bool exprComp() {
    if (exprAdd()) {
        if (consume(LESS)) {
            Ret leftType=ret;
            Text_write(crtCode,"<");
            if (exprAdd()) {
                if(leftType.type!=ret.type)tkerr("different types for the operands of < or ==");
                setRet(TYPE_INT,false);
            } else tkerr("missing expression after '<'");
        } else if (consume(LESSEQ)) {
            Ret leftType=ret;
            Text_write(crtCode,"<=");
            if (exprAdd()) {
                if(leftType.type!=ret.type)tkerr("different types for the operands of < or ==");
                setRet(TYPE_INT,false);
            } else tkerr("missing expression after '<='");
        } else if (consume(EQUAL)) {
            Ret leftType=ret;
            Text_write(crtCode,"==");
            if (exprAdd()) {
                if(leftType.type!=ret.type)tkerr("different types for the operands of < or ==");
                setRet(TYPE_INT,false);
            } else tkerr("missing expression after '=='");
        } else if (consume(NOTEQ)) {
            Ret leftType=ret;
            Text_write(crtCode,"!=");
            if (exprAdd()) {
                if(leftType.type!=ret.type)tkerr("different types for the operands of < or ==");
                setRet(TYPE_INT,false);
            } else tkerr("missing expression after '!='");
        } else if (consume(GREATER)) {
            Ret leftType=ret;
            Text_write(crtCode,">");
            if (exprAdd()) {
                if(leftType.type!=ret.type)tkerr("different types for the operands of < or ==");
                setRet(TYPE_INT,false);
            } else tkerr("missing expression after '>'");
        } else if (consume(GREATEREQ)) {
            Ret leftType=ret;
            Text_write(crtCode,">=");
            if (exprAdd()) {
                if(leftType.type!=ret.type)tkerr("different types for the operands of < or ==");
                setRet(TYPE_INT,false);
            } else tkerr("missing expression after '>='");
        }
        return true;
    }
    return false;
}

// exprAssign ::= ( ID ASSIGN )? exprComp
bool exprAssign() {
    int start = iTk;
    if (consume(ID)) {
        const char *name=consumed->text;
        if (consume(ASSIGN)) {
            Text_write(crtCode,"%s=",name);
            if (exprComp()) {
                Symbol *s=searchSymbol(name);
                if(!s)tkerr("undefined symbol: %s",name);
                if(s->kind==KIND_FN)tkerr("a function (%s) cannot be used as a destination for assignment ",name);
                if(s->type!=ret.type)tkerr("the source and destination for assignment must have the same type");
                ret.lval=false;
                return true;
            }
            tkerr("missing expression after '='");
        }
        iTk = start;
    }
    if (exprComp()) {
        return true;
    }
    iTk = start;
    return false;
}

// exprLogic ::= exprAssign ( ( AND | OR ) exprAssign )*
bool exprLogic() {
    if (exprAssign()) {
        for (;;) {
            if (consume(AND)) {
                Ret leftType=ret;
                if(leftType.type==TYPE_STR)tkerr("the left operand of && or || cannot be of type str");
                Text_write(crtCode,"&&");
                if (exprAssign()) {
                    if(ret.type==TYPE_STR)tkerr("the right operand of && or || cannot be of type str");
                    setRet(TYPE_INT,false);
                } else tkerr("missing expression after '&&'");
            } else if (consume(OR)) {
                Ret leftType=ret;
                if(leftType.type==TYPE_STR)tkerr("the left operand of && or || cannot be of type str");
                Text_write(crtCode,"||");
                if (exprAssign()) {
                    if(ret.type==TYPE_STR)tkerr("the right operand of && or || cannot be of type str");
                    setRet(TYPE_INT,false);
                } else tkerr("missing expression after '||'");
            } else break;
        }
        return true;
    }
    return false;
}

// expr ::= exprLogic
bool expr() {
    return exprLogic();
}

// factor ::= INT | REAL | STR | LPAR expr RPAR | ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
bool factor() {
    int start = iTk;

    if (consume(INT)) {
        setRet(TYPE_INT,false);
        Text_write(crtCode,"%d",consumed->i);
        return true;
    }
    if (consume(REAL)) {
        setRet(TYPE_REAL,false);
        Text_write(crtCode,"%g",consumed->r);
        return true;
    }
    if (consume(STR)) {
        setRet(TYPE_STR,false);
        Text_write(crtCode,"\"%s\"",consumed->text);
        return true;
    }

    // LPAR expr RPAR
    if (consume(LPAR)) {
        Text_write(crtCode,"(");
        if (expr()) {
            if (consume(RPAR)) {
                Text_write(crtCode,")");
                return true;
            }
            tkerr("missing ')' after expression");
        }
        tkerr("missing expression after '('");
    }

    // ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
    if (consume(ID)) {
        Symbol *s=searchSymbol(consumed->text);
        if(!s)tkerr("undefined symbol: %s",consumed->text);
        Text_write(crtCode,"%s",s->name);
        if (consume(LPAR)) {
            if(s->kind!=KIND_FN)tkerr("%s cannot be called, because it is not a function",s->name);
            Symbol *argDef=s->args;
            Text_write(crtCode,"(");
            // Function call
            if (expr()) {
                if(!argDef)tkerr("the function %s is called with too many arguments",s->name);
                if(argDef->type!=ret.type)tkerr("the argument type at function %s call is different from the one given at its definition",s->name);
                argDef=argDef->next;
                while (consume(COMMA)) {
                    Text_write(crtCode,",");
                    if (expr()) {
                        if(!argDef)tkerr("the function %s is called with too many arguments",s->name);
                        if(argDef->type!=ret.type)tkerr("the argument type at function %s call is different from the one given at its definition",s->name);
                        argDef=argDef->next;
                    } else tkerr("missing expression after ','");
                }
            }
            if (consume(RPAR)) {
                if(argDef)tkerr("the function %s is called with too few arguments",s->name);
                setRet(s->type,false);
                Text_write(crtCode,")");
            } else tkerr("missing ')' after function arguments");
        } else {
            if(s->kind==KIND_FN)
            tkerr("the function %s can only be called",s->name);
            setRet(s->type,true);
        }
        // else: just an ID (variable reference)
        return true;
    }

    iTk = start;
    return false;
}

// exprPrefix ::= ( SUB | NOT )? factor
bool exprPrefix() {
    int start = iTk;
    if (consume(SUB)) {
        Text_write(crtCode,"-");
        if (factor()) {
            if(ret.type==TYPE_STR)tkerr("the expression of unary - must be of type int or real");
            ret.lval=false;
            return true;
        }
        tkerr("missing expression after '-'");
    }
    if (consume(NOT)) {
        Text_write(crtCode,"!");
        if (factor()) {
            if(ret.type==TYPE_STR)tkerr("the expression of ! must be of type int or real");
            setRet(TYPE_INT,false);
            return true;
        }
        tkerr("missing expression after '!'");
    }
    if (factor())
        return true;
    iTk = start;
    return false;
}

// program ::= ( defVar | defFunc | block )* FINISH
bool program() {
    addDomain();
    addPredefinedFns();

    crtCode=&tMain;
    crtVar=&tBegin;
    Text_write(&tBegin,"#include \"quick.h\"\n\n");
    Text_write(&tMain,"\nint main(){\n");

    for (;;) {
        if (defVar()) {
        } else if (defFunc()) {
        } else if (block()) {
        } else break;
    }
    if (consume(FINISH)) {
        Text_write(&tMain,"return 0;\n}\n");
        FILE *fis=fopen("1.c","w");
        if(!fis){
            printf("cannot write to file 1.c\n");
            exit(EXIT_FAILURE);
        }
        fwrite(tBegin.buf,sizeof(char),tBegin.n,fis);
        fwrite(tFunctions.buf,sizeof(char),tFunctions.n,fis);
        fwrite(tMain.buf,sizeof(char),tMain.n,fis);
        fclose(fis);
        delDomain();
        return true;
    }

    tkerr("syntax error");
    return false;
}

void parse() {
    iTk = 0;
    if (program()) {
    } else tkerr("syntax error in program");
}
