%token BREAK CONTINUE DO ELSE FOR IF RETURN WHILE FUNCTION IN NULL_TOKEN
%token TRUE_TOKEN FALSE_TOKEN VAR CLASS IS STATIC SUPER THIS IMPORT FROM
%token IDENTIFIER CONSTANT STRING_LITERAL
%token RIGHT_ASSIGN LEFT_ASSIGN ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN
%token MOD_ASSIGN AND_ASSIGN XOR_ASSIGN OR_ASSIGN NULL_ASSIGN
%token RIGHT_OP LEFT_OP INC_OP DEC_OP AND_OP OR_OP
%token LE_OP GE_OP EQ_OP NE_OP

%token SEMI OPEN_BRACE CLOSE_BRACE COMMA COLON ASSIGN OPEN_PAREN CLOSE_PAREN
%token OPEN_BRACKET CLOSE_BRACKET BIT_AND LOGICAL_NOT BIT_NOT
%token MINUS PLUS ASTERISK DIVIDE MODULO LESS_THAN GREATER_THAN XOR BIT_OR
%token QUESTION DOT DOTDOT DOTDOTDOT

%start translation_unit

%%

translation_unit
    : external_declaration
    | translation_unit external_declaration
    ;

external_declaration
    : function_definition
    | class_definition
    | import_statement
    | statement
    ;

function_definition
    : FUNCTION IDENTIFIER OPEN_PAREN identifier_list CLOSE_PAREN compound_statement
    | STATIC FUNCTION IDENTIFIER OPEN_PAREN identifier_list CLOSE_PAREN compound_statement
    ;

class_definition
    : CLASS IDENTIFIER class_body
    | CLASS IDENTIFIER IS IDENTIFIER class_body
    ;

import_statement
    : IMPORT module_name SEMI
    | FROM module_name IMPORT identifier_list SEMI
    | FROM module_name IMPORT ASTERISK SEMI
    ;

statement
    : variable_definition
    | expression_statement
    | selection_statement
    | iteration_statement
    | jump_statement
    ;

identifier_list
    : IDENTIFIER
    | identifier_list COMMA IDENTIFIER
    |
    ;

compound_statement
    : OPEN_BRACE CLOSE_BRACE
    | OPEN_BRACE statement_list CLOSE_BRACE
    ;

class_body
    : OPEN_BRACE CLOSE_BRACE
    | OPEN_BRACE class_member_list CLOSE_BRACE
    ;

module_name
    : IDENTIFIER
    | module_name DOT IDENTIFIER
    ;

variable_definition
    : variable_declaration
    | variable_specifier ASSIGN expression SEMI
    ;

expression_statement
    : SEMI
    | expression SEMI
    ;

selection_statement
    : IF OPEN_PAREN expression CLOSE_PAREN compound_statement ELSE selection_statement
    | IF OPEN_PAREN expression CLOSE_PAREN compound_statement ELSE compound_statement
    | IF OPEN_PAREN expression CLOSE_PAREN compound_statement
    ;

iteration_statement
    : WHILE OPEN_PAREN expression CLOSE_PAREN compound_statement
    | DO compound_statement WHILE OPEN_PAREN expression CLOSE_PAREN SEMI
    | FOR OPEN_PAREN IDENTIFIER IN expression CLOSE_PAREN compound_statement
    | FOR OPEN_PAREN expression_statement expression_statement CLOSE_PAREN compound_statement
    | FOR OPEN_PAREN expression_statement expression_statement expression CLOSE_PAREN compound_statement
    ;

jump_statement
    : CONTINUE SEMI
    | BREAK SEMI
    | RETURN SEMI
    | RETURN expression SEMI
    ;

statement_list
    : statement
    | statement_list statement
    ;

class_member_list
    : class_member
    | class_member_list class_member
    ;

variable_declaration
    : variable_specifier SEMI
    ;

variable_specifier
    : STATIC VAR IDENTIFIER
    | VAR IDENTIFIER
    ;

expression
    : assignment_expression
    | expression COMMA assignment_expression
    ;

class_member
    : function_definition
    | variable_declaration
    ;

assignment_expression
    : conditional_expression
    | unary_expression assignment_operator assignment_expression
    ;

conditional_expression
    : logical_or_expression
    | logical_or_expression QUESTION expression COLON conditional_expression
    ;

unary_expression
    : postfix_expression
    | INC_OP unary_expression
    | DEC_OP unary_expression
    | unary_operator unary_expression
    ;

assignment_operator
    : ASSIGN
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

logical_or_expression
    : logical_and_expression
    | logical_or_expression OR_OP logical_and_expression
    ;

postfix_expression
    : primary_expression
    | postfix_expression DOT IDENTIFIER
    | postfix_expression OPEN_BRACKET expression CLOSE_BRACKET
    | postfix_expression OPEN_PAREN argument_expression_list CLOSE_PAREN
    | postfix_expression INC_OP
    | postfix_expression DEC_OP
    ;

unary_operator
    : PLUS
    | MINUS
    | BIT_NOT
    | LOGICAL_NOT
    ;

logical_and_expression
    : equality_expression
    | logical_and_expression AND_OP equality_expression
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
    | OPEN_PAREN expression CLOSE_PAREN
    ;

argument_expression_list
    : assignment_expression
    | argument_expression_list COMMA assignment_expression
    |
    ;

equality_expression
    : relational_expression
    | equality_expression IS relational_expression
    | equality_expression EQ_OP relational_expression
    | equality_expression NE_OP relational_expression
    ;

dictionary
    : OPEN_BRACE CLOSE_BRACE
    | OPEN_BRACE dictionary_element_list CLOSE_BRACE
    | OPEN_BRACE dictionary_element_list COMMA CLOSE_BRACE
    ;

list
    : OPEN_BRACKET CLOSE_BRACKET
    | OPEN_BRACKET list_element_list CLOSE_BRACKET
    | OPEN_BRACKET list_element_list COMMA CLOSE_BRACKET
    ;

relational_expression
    : inclusive_or_expression
    | relational_expression LESS_THAN inclusive_or_expression
    | relational_expression GREATER_THAN inclusive_or_expression
    | relational_expression LE_OP inclusive_or_expression
    | relational_expression GE_OP inclusive_or_expression
    ;

dictionary_element_list
    : dictionary_element
    | dictionary_element_list COMMA dictionary_element
    ;

list_element_list
    : conditional_expression
    | list_element_list COMMA conditional_expression
    ;

inclusive_or_expression
    : exclusive_or_expression
    | inclusive_or_expression BIT_OR exclusive_or_expression
    ;

dictionary_element
    : expression COLON conditional_expression
    ;

exclusive_or_expression
    : and_expression
    | exclusive_or_expression XOR and_expression
    ;

and_expression
    : shift_expression
    | and_expression BIT_AND shift_expression
    ;

shift_expression
    : range_expression
    | shift_expression LEFT_OP range_expression
    | shift_expression RIGHT_OP range_expression
    ;

range_expression
    : additive_expression
    | range_expression DOTDOT additive_expression
    | range_expression DOTDOTDOT additive_expression
    ;

additive_expression
    : multiplicative_expression
    | additive_expression PLUS multiplicative_expression
    | additive_expression MINUS multiplicative_expression
    ;

multiplicative_expression
    : unary_expression
    | multiplicative_expression ASTERISK unary_expression
    | multiplicative_expression DIVIDE unary_expression
    | multiplicative_expression MODULO unary_expression
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
