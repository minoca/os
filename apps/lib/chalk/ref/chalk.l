D           [0-9]
L           [a-zA-Z_]
H           [a-fA-F0-9]

%{
#include <stdio.h>
#include "y.tab.h"

void count();
%}

%%
"/*"            { comment(); }

"break"         { count(); return(BREAK); }
"continue"      { count(); return(CONTINUE); }
"do"            { count(); return(DO); }
"else"          { count(); return(ELSE); }
"for"           { count(); return(FOR); }
"if"            { count(); return(IF); }
"return"        { count(); return(RETURN); }
"while"         { count(); return(WHILE); }
"function"      { count(); return(FUNCTION); }

{L}({L}|{D})*       { count(); return(IDENTIFIER); }

0[xX]{H}+?      { count(); return(CONSTANT); }
0{D}+?      { count(); return(CONSTANT); }
{D}+?       { count(); return(CONSTANT); }

L?\"(\\.|[^\\"])*\" { count(); return(STRING_LITERAL); }

">>="           { count(); return(RIGHT_ASSIGN); }
"<<="           { count(); return(LEFT_ASSIGN); }
"+="            { count(); return(ADD_ASSIGN); }
"-="            { count(); return(SUB_ASSIGN); }
"*="            { count(); return(MUL_ASSIGN); }
"/="            { count(); return(DIV_ASSIGN); }
"%="            { count(); return(MOD_ASSIGN); }
"&="            { count(); return(AND_ASSIGN); }
"^="            { count(); return(XOR_ASSIGN); }
"|="            { count(); return(OR_ASSIGN); }
">>"            { count(); return(RIGHT_OP); }
"<<"            { count(); return(LEFT_OP); }
"++"            { count(); return(INC_OP); }
"--"            { count(); return(DEC_OP); }
"&&"            { count(); return(AND_OP); }
"||"            { count(); return(OR_OP); }
"<="            { count(); return(LE_OP); }
">="            { count(); return(GE_OP); }
"=="            { count(); return(EQ_OP); }
"!="            { count(); return(NE_OP); }

";"         { count(); return(';'); }
("{")       { count(); return('{'); }
("}")       { count(); return('}'); }
","         { count(); return(','); }
":"         { count(); return(':'); }
"="         { count(); return('='); }
"("         { count(); return('('); }
")"         { count(); return(')'); }
("[")       { count(); return('['); }
("]")       { count(); return(']'); }
"&"         { count(); return('&'); }
"!"         { count(); return('!'); }
"~"         { count(); return('~'); }
"-"         { count(); return('-'); }
"+"         { count(); return('+'); }
"*"         { count(); return('*'); }
"/"         { count(); return('/'); }
"%"         { count(); return('%'); }
"<"         { count(); return('<'); }
">"         { count(); return('>'); }
"^"         { count(); return('^'); }
"|"         { count(); return('|'); }
"?"         { count(); return('?'); }

[ \t\v\n\f]     { count(); }
.           { /* ignore bad characters */ }

%%

yywrap()
{
    return(1);
}


comment()
{
    char c, c1;

loop:
    while ((c = input()) != '*' && c != 0)
        putchar(c);

    if ((c1 = input()) != '/' && c != 0)
    {
        unput(c1);
        goto loop;
    }

    if (c != 0)
        putchar(c1);
}


int column = 0;

void count()
{
    int i;

    for (i = 0; yytext[i] != '\0'; i++)
        if (yytext[i] == '\n')
            column = 0;
        else if (yytext[i] == '\t')
            column += 8 - (column % 8);
        else
            column++;

    ECHO;
}
