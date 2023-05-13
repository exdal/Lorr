%require "3.7"
%define parse.error verbose

%code requires 
{
#include "Parser.hh"
#include "lm_type.hh"
}

%{
#include "lm_bison.hh"
#include "lm_flex.hh"
void yyerror(YYLTYPE *pYYL, lm::Result *pResult, yyscan_t scanner, const char *pMessage)
{
    pResult->PushError(pYYL->first_line, pYYL->first_column, pMessage);
}
%}

%define api.pure full
%define api.prefix {lm_}
%define api.token.prefix {LM_}

%locations

%lex-param { yyscan_t scanner }
%parse-param { lm::Result *pResult }
%parse-param { yyscan_t scanner }

%union
{
    char *string;
    double numberf;
    i64 numberi;
}

%token LCURLY RCURLY RBRACKET LBRACKET COMMA ASSIGN
%token TRUE FALSE

%token <string> IDENTIFIER STRING
%token <numberf> FLOAT
%token <numberi> NUMBER

%left LCURLY LBRACKET
%right RCURLY RBRACKET
%%

LM
:
| categories
| members
;

object
: LCURLY         RCURLY { pResult->PopCategory(); } //  { }
| LCURLY members RCURLY { pResult->PopCategory(); } //  {Members..}
;

array
: RBRACKET  		   LBRACKET { pResult->PopArray(); } // []
| RBRACKET arrayValues LBRACKET { pResult->PopArray(); } // [values...]
;

value 
:
| STRING { pResult->PushString($1); }   // "String"
| NUMBER { pResult->PushNumber($1); }   // 12345
| FLOAT { pResult->PushDouble($1); }    // 1.2345
| TRUE { pResult->PushBool(true); }     // true
| FALSE { pResult->PushBool(false); }   // false
;

arrayValues 
: value
| arrayValues COMMA value
;

members 
: member
| members member
| members category
| members list
| list;

categories 
: category
| categories category;

category: IDENTIFIER { pResult->PushCategory($1); } object;
member: IDENTIFIER ASSIGN { pResult->PushVar($1); } value;
list: IDENTIFIER ASSIGN { pResult->PushArray($1); } array;

%%

void yyerror(const char *s);