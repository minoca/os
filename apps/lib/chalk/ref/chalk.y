%token IDENTIFIER CONSTANT STRING_LITERAL

%token INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN
%token XOR_ASSIGN OR_ASSIGN

%token RETURN

%token FUNCTION

%start translation_unit

%%

list_element_list
    : conditional_expression
    | list_element_list ',' conditional_expression
    ;

list
    : '[' ']'
    | '[' list_element_list ']'
    | '[' list_element_list ',' ']'
    ;

dictionary_element
    : expression ':' conditional_expression
    ;

dictionary_element_list
    : dictionary_element
    | dictionary_element_list ',' dictionary_element
    ;

dictionary
    : '{' '}'
    | '{' dictionary_element_list '}'
    | '{' dictionary_element_list ',' '}'
    ;

primary_expression
    : IDENTIFIER
    | CONSTANT
    | STRING_LITERAL
    | dictionary
    | list
    | '(' expression ')'
    ;

postfix_expression
    : primary_expression
    | postfix_expression '[' expression ']'
    | postfix_expression '(' argument_expression_list ')'
    | postfix_expression INC_OP
    | postfix_expression DEC_OP
    ;
    
argument_expression_list
    : assignment_expression
    | argument_expression_list ',' assignment_expression
    | 
    ;

unary_expression
    : postfix_expression
    | INC_OP unary_expression
    | DEC_OP unary_expression
    | unary_operator unary_expression
    ;

unary_operator
    : '+'
    | '-'
    | '~'
    | '!'
    ;

multiplicative_expression
    : unary_expression
    | multiplicative_expression '*' unary_expression
    | multiplicative_expression '/' unary_expression
    | multiplicative_expression '%' unary_expression
    ;

additive_expression
    : multiplicative_expression
    | additive_expression '+' multiplicative_expression
    | additive_expression '-' multiplicative_expression
    ;

shift_expression
    : additive_expression
    | shift_expression LEFT_OP additive_expression
    | shift_expression RIGHT_OP additive_expression
    ;

relational_expression
    : shift_expression
    | relational_expression '<' shift_expression
    | relational_expression '>' shift_expression
    | relational_expression LE_OP shift_expression
    | relational_expression GE_OP shift_expression
    ;

equality_expression
    : relational_expression
    | equality_expression EQ_OP relational_expression
    | equality_expression NE_OP relational_expression
    ;

and_expression
    : equality_expression
    | and_expression '&' equality_expression
    ;

exclusive_or_expression
    : and_expression
    | exclusive_or_expression '^' and_expression
    ;

inclusive_or_expression
    : exclusive_or_expression
    | inclusive_or_expression '|' exclusive_or_expression
    ;

logical_and_expression
    : inclusive_or_expression
    | logical_and_expression AND_OP inclusive_or_expression
    ;

logical_or_expression
    : logical_and_expression
    | logical_or_expression OR_OP logical_and_expression
    ;

conditional_expression
    : logical_or_expression
    | logical_or_expression '?' expression ':' conditional_expression
    ;

assignment_expression
    : conditional_expression
    | unary_expression assignment_operator assignment_expression
    ;

assignment_operator
    : '='
    | MUL_ASSIGN
    | DIV_ASSIGN
    | MOD_ASSIGN
    | ADD_ASSIGN
    | SUB_ASSIGN
    | LEFT_ASSIGN
    | RIGHT_ASSIGN
    | AND_ASSIGN
    | XOR_ASSIGN
    | OR_ASSIGN
    ;

expression
    : assignment_expression
    | expression ',' assignment_expression
    ;

statement
    : expression_statement
    | jump_statement
    ;

compound_statement
    : '{' '}'
    | '{' statement_list '}'
    ;

statement_list
    : statement
    | statement_list statement
    ;

expression_statement
    : ';'
    | expression ';'
    ;

jump_statement
    : RETURN ';'
    | RETURN expression ';'
    ;

translation_unit
    : external_declaration
    | translation_unit external_declaration
    ;

external_declaration
    : function_definition
    | statement
    ;

identifier_list
    : IDENTIFIER
    | identifier_list ',' IDENTIFIER
    ;

function_definition
    : FUNCTION IDENTIFIER '(' ')' compound_statement
    | FUNCTION IDENTIFIER '(' identifier_list ')' compound_statement
    ;

%%
#include <stdio.h>

extern char yytext[];
extern int column;

int yydebug=1;

yyerror(s)
char *s;
{
    fflush(stdout);
    printf("\n%*s\n%*s\n", column, "^", column, s);
}
int main()
{
  yyparse();
  return 0;
}
