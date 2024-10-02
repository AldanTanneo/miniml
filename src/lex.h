#pragma once

#include <stdio.h>

#include "utils.h"

typedef enum {
  T_ERROR = -2,
  T_INCOMPLETE = -1,
  T_NUM = 0,
  T_STR,
  T_IDENT,
  T_LET,
  T_REC,
  T_FUN,
  T_IN,
  T_IF,
  T_THEN,
  T_ELSE,
  T_LPAR,
  T_RPAR,
  T_PLUS,
  T_MINUS,
  T_STAR,
  T_SLASH,
  T_EQ,
  T_EQEQ,
  T_SEMI,
  T_ARROW,
  T_SEMISEMI,
  T_TRUE,
  T_FALSE,
} tkind_t;

typedef struct token {
  tkind_t kind;
  union {
    f64 num;
    str_t str;
    str_t ident;
    const char *error;
  };
} token_t;

typedef struct tokenbuf {
  token_t *tokens;
  usize len;
  usize cap;
} tokenbuf_t;

tokenbuf_t tokenbuf_new();
void tokenbuf_push(tokenbuf_t *tokens, token_t tok);

typedef struct {
  const u8 *data;
  usize len;
  usize seen;
} lexstream_t;

lexstream_t lexstream_new(const u8 *data, usize len);
token_t lex(lexstream_t *stream);
void fprint_token(FILE *f, token_t *tok);

typedef token_t (*lexer_t)(lexstream_t *);
