// Created on Friday May 12th 2023 by exdal
// Last modified on Wednesday May 17th 2023 by exdal
/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_LM_LM_BISON_HH_INCLUDED
# define YY_LM_LM_BISON_HH_INCLUDED
/* Debug traces.  */
#ifndef LM_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define LM_DEBUG 1
#  else
#   define LM_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define LM_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined LM_DEBUG */
#if LM_DEBUG
extern int lm_debug;
#endif
/* "%code requires" blocks.  */
#line 5 "parser.y"

#include "Parser.hh"
#include "lm_type.hh"

#line 62 "lm_bison.hh"

/* Token kinds.  */
#ifndef LM_TOKENTYPE
# define LM_TOKENTYPE
  enum lm_tokentype
  {
    LM_LM_EMPTY = -2,
    LM_YYEOF = 0,                  /* "end of file"  */
    LM_LM_error = 256,             /* error  */
    LM_LM_UNDEF = 257,             /* "invalid token"  */
    LM_LCURLY = 258,               /* LCURLY  */
    LM_RCURLY = 259,               /* RCURLY  */
    LM_RBRACKET = 260,             /* RBRACKET  */
    LM_LBRACKET = 261,             /* LBRACKET  */
    LM_COMMA = 262,                /* COMMA  */
    LM_ASSIGN = 263,               /* ASSIGN  */
    LM_TRUE = 264,                 /* TRUE  */
    LM_FALSE = 265,                /* FALSE  */
    LM_IDENTIFIER = 266,           /* IDENTIFIER  */
    LM_STRING = 267,               /* STRING  */
    LM_FLOAT = 268,                /* FLOAT  */
    LM_NUMBER = 269                /* NUMBER  */
  };
  typedef enum lm_tokentype lm_token_kind_t;
#endif

/* Value type.  */
#if ! defined LM_STYPE && ! defined LM_STYPE_IS_DECLARED
union LM_STYPE
{
#line 30 "parser.y"

    char *string;
    double numberf;
    i64 numberi;

#line 99 "lm_bison.hh"

};
typedef union LM_STYPE LM_STYPE;
# define LM_STYPE_IS_TRIVIAL 1
# define LM_STYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined LM_LTYPE && ! defined LM_LTYPE_IS_DECLARED
typedef struct LM_LTYPE LM_LTYPE;
struct LM_LTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define LM_LTYPE_IS_DECLARED 1
# define LM_LTYPE_IS_TRIVIAL 1
#endif




int lm_parse (lm::Result *pResult, yyscan_t scanner);


#endif /* !YY_LM_LM_BISON_HH_INCLUDED  */
