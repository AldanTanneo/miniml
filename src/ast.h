#pragma once

#include "lex.h"

struct expr;
typedef struct expr expr_t;

typedef struct error_chain {
  str_t msg;
  struct error_chain *next;
} error_chain_t;

typedef enum exprkind {
  E_NOMATCH = -2,
  E_ERROR = -1,
  E_NUM,
  E_STR,
  E_BOOL,
  E_UNIT,
  E_VAR,
  E_CALL,
  E_LET,
  E_LETREC,
  E_FUN,
  E_IFTHEN,
  E_NEG,
  E_ADD,
  E_SUB,
  E_MUL,
  E_DIV,
  E_EQ,
} exprkind_t;

typedef struct evar {
  str_t name;
} evar_t;

typedef struct ecall {
  expr_t *callee;
  expr_t *param;
} ecall_t;

typedef struct elet {
  str_t name;
  expr_t *expr;
  expr_t *body;
} elet_t;

typedef struct efun {
  str_t param;
  expr_t *body;
} efun_t;

typedef struct eif {
  expr_t *cond;
  expr_t *then_body;
  expr_t *else_body;
} eif_t;

typedef struct eunop {
  expr_t *rhs;
} eunop_t;

typedef struct ebinop {
  expr_t *lhs;
  expr_t *rhs;
} ebinop_t;

struct expr {
  exprkind_t kind;
  union {
    error_chain_t error;
    f64 num;
    str_t str;
    bool boolean;
    evar_t var;
    ecall_t call;
    elet_t let;
    efun_t fun;
    eif_t ifthen;
    eunop_t unop;
    ebinop_t binop;
  };
};

typedef enum toplevelkind {
  TL_ERROR = E_ERROR,
  TL_LET,
  TL_LETREC,
  TL_EXPR,
} toplevelkind_t;

typedef struct toplevel_let {
  str_t name;
  expr_t expr;
} toplevel_let_t;

typedef struct toplevel {
  toplevelkind_t kind;
  union {
    error_chain_t error;
    toplevel_let_t let;
    expr_t expr;
  };
} toplevel_t;

typedef struct parser {
  token_t *tokens;
  usize len;
  usize pos;
} parser_t;

parser_t parser_new(tokenbuf_t tokens);
expr_t expr(parser_t *parser);
toplevel_t toplevel(parser_t *toplevel);

void fprint_expr(FILE *f, expr_t *e);
void fprint_toplevel(FILE *f, toplevel_t *tl);
