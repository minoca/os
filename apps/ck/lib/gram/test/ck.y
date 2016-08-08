%token IDENTIFIER CONSTANT STRING_LITERAL

%token INC_OP DEC_OP LEFT_OP RIGHT_OP LE_OP GE_OP EQ_OP NE_OP
%token AND_OP OR_OP MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN ADD_ASSIGN
%token SUB_ASSIGN LEFT_ASSIGN RIGHT_ASSIGN AND_ASSIGN
%token XOR_ASSIGN OR_ASSIGN NULL_ASSIGN

%token BREAK CONTINUE DO FOR ELSE IF RETURN WHILE

%token FUNCTION IN NULL_TOKEN DOTDOT DOTDOTDOT CLASS STATIC SUPER THIS IS VAR
%token FROM IMPORT
%token TRUE_TOKEN FALSE_TOKEN

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
    | NULL_TOKEN
    | THIS
    | SUPER
    | TRUE_TOKEN
    | FALSE_TOKEN
    | dictionary
    | list
    | '(' expression ')'
    ;

postfix_expression
    : primary_expression
    | postfix_expression '.' IDENTIFIER '(' argument_expression_list ')'
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
    : '-'
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

range_expression
    : additive_expression
    | range_expression DOTDOT additive_expression
    | range_expression DOTDOTDOT additive_expression
    ;

shift_expression
    : range_expression
    | shift_expression LEFT_OP range_expression
    | shift_expression RIGHT_OP range_expression
    ;

and_expression
    : shift_expression
    | and_expression '&' shift_expression
    ;

exclusive_or_expression
    : and_expression
    | exclusive_or_expression '^' and_expression
    ;

inclusive_or_expression
    : exclusive_or_expression
    | inclusive_or_expression '|' exclusive_or_expression
    ;

relational_expression
    : inclusive_or_expression
    | relational_expression '<' inclusive_or_expression
    | relational_expression '>' inclusive_or_expression
    | relational_expression LE_OP inclusive_or_expression
    | relational_expression GE_OP inclusive_or_expression
    ;

equality_expression
    : relational_expression
    | equality_expression IS relational_expression
    | equality_expression EQ_OP relational_expression
    | equality_expression NE_OP relational_expression
    ;

logical_and_expression
    : equality_expression
    | logical_and_expression AND_OP equality_expression
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
    | NULL_ASSIGN
    ;

expression
    : assignment_expression
    | expression ',' assignment_expression
    ;

variable_specifier
    : STATIC VAR IDENTIFIER
    | VAR IDENTIFIER
    ;

variable_declaration
    : variable_specifier ';'
    ;

variable_definition
    : variable_declaration
    | variable_specifier '=' expression ';'
    ;

statement
    : function_definition
    | variable_definition
    | expression_statement
    | selection_statement
    | iteration_statement
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

selection_statement
    : IF '(' expression ')' compound_statement ELSE selection_statement
    | IF '(' expression ')' compound_statement ELSE compound_statement
    | IF '(' expression ')' compound_statement
    ;

iteration_statement
    : WHILE '(' expression ')' compound_statement
    | DO compound_statement WHILE '(' expression ')' ';'
    | FOR '(' IDENTIFIER IN expression ')' compound_statement
    | FOR '(' statement expression ';' ')' compound_statement
    | FOR '(' statement expression ';' expression ')' compound_statement
    ;

jump_statement
    : CONTINUE ';'
    | BREAK ';'
    | RETURN ';'
    | RETURN expression ';'
    ;

identifier_list
    : IDENTIFIER
    | identifier_list ',' IDENTIFIER
    |
    ;

function_definition
    : FUNCTION IDENTIFIER '(' identifier_list ')' compound_statement
    | STATIC FUNCTION IDENTIFIER '(' identifier_list ')' compound_statement
    ;

class_member
    : function_definition
    | variable_declaration
    ;

class_member_list
    : class_member
    | class_member_list class_member
    ;

class_body
    : '{' '}'
    | '{' class_member_list '}'
    ;

class_definition
    : CLASS IDENTIFIER class_body
    | CLASS IDENTIFIER IS expression class_body
    ;

module_name
    : IDENTIFIER
    | module_name '.' IDENTIFIER
    ;

import_statement
    : IMPORT module_name ';'
    | FROM module_name IMPORT identifier_list ';'
    | FROM module_name IMPORT '*' ';'
    ;

external_declaration
    : class_definition
    | import_statement
    | statement
    ;

translation_unit
    : external_declaration
    | translation_unit external_declaration
    ;

%%
#include <stdio.h>
#include <errno.h>
#include <string.h>

extern char yytext[];
extern int column;
extern FILE *yyin;

int yydebug=1;

yyerror(s)
char *s;
{
    fflush(stdout);
    printf("\n%*s\n%*s\n", column, "^", column, s);
}
int main()
int main(int argc, char **argv)
{

  if (argc > 2) {
    printf("Usage: %s <file>\n", argv[0]);
    return 1;
  }

  if (argc == 2) {
    yyin = fopen(argv[1], "r");
    if (!yyin) {
      fprintf(stderr, "Error: Cannot open %s: %s\n", argv[1], strerror(errno));
      return 1;
    }
  }

  yyparse();
  fclose(yyin);
  return 0;
}
