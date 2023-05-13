%option noinput nounput noyywrap 8bit reentrant bison-bridge
%option never-interactive
%option batch
%option nounput
%option warn
%option case-insensitive
%option prefix="lm_"
%option bison-locations

%{
#include "lm_bison.hh"

#define TOKEN(name) { return LM_##name; }
%}

%%

(#.*)                                ; { /* Ignore Comments */ }
[ \t\r\n]+                           ; { /* Ignore whitespaces */ }

"true"                  TOKEN(TRUE)
"false"                 TOKEN(FALSE)

\"(?:\\.|[^\"\\])*\"                 { yylval->string  = strdup(yytext); return LM_STRING; }
[A-Za-z_][A-Za-z_0-9]+               { yylval->string  = strdup(yytext); return LM_IDENTIFIER; }
0[xX][0-9a-fA-F]+                    { yylval->numberi = (i64)strtoll(yytext, 0, 0); return LM_NUMBER; }
[+-]?[0-9]+                          { yylval->numberi = atoi(yytext); return LM_NUMBER; }
[+-]?([0-9]*[.])?[0-9]+              { yylval->numberf = atof(yytext); return LM_FLOAT; }

\{                      TOKEN(LCURLY)
\}                      TOKEN(RCURLY)
\=                      TOKEN(ASSIGN)
\[                      TOKEN(RBRACKET)
\]                      TOKEN(LBRACKET)
\,                      TOKEN(COMMA)

%%
