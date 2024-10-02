
#include "ast.h"
#include "utils.h"
#include <stdio.h>

#define PARSER(NAME, args...)                                                  \
  expr_t NAME(parser_t *__parser, ##args) {                                    \
    goto __start;                                                              \
  __start:

#define END_PARSER() }

#define PEEK() (parser_peek(__parser))
#define NEXT() (parser_next(__parser))
#define CALL(parser, args...) ((parser)(__parser, ##args))
#define ERROR(__msg)                                                           \
  ((expr_t){.kind = E_ERROR,                                                   \
            .error = (error_chain_t){.msg = STR(__msg), .next = NULL}})
#define NOMATCH() ((expr_t){.kind = E_NOMATCH})
#define ASSERT(__expr, __msg)                                                  \
  {                                                                            \
    if (__expr.kind == E_ERROR) {                                              \
      error_chain_t *next = gcalloc(sizeof(error_chain_t));                    \
      *next = __expr.error;                                                    \
      error_chain_t error = (error_chain_t){.msg = __msg, .next = next};       \
      return (expr_t){.kind = E_ERROR, .error = error};                        \
    }                                                                          \
  }

static inline token_t parser_peek(parser_t *parser) {
  if (parser->pos < parser->len) {
    token_t res = parser->tokens[parser->pos];
    return res;
  } else {
    return (token_t){.kind = T_INCOMPLETE};
  }
}

static inline token_t parser_next(parser_t *parser) {
  if (parser->pos < parser->len) {
    token_t res = parser->tokens[parser->pos];
    parser->pos += 1;
    return res;
  } else {
    return (token_t){.kind = T_INCOMPLETE};
  }
}

parser_t parser_new(tokenbuf_t tokens) {
  if (tokens.len == 0) {
    return (parser_t){.tokens = NULL, .len = 0, .pos = 0};
  } else {
    token_t *buf = gcrealloc(tokens.tokens, tokens.len * sizeof(token_t));
    return (parser_t){.tokens = buf, .len = tokens.len, .pos = 0};
  }
}

PARSER(atom) {
  token_t next = PEEK();
  switch (next.kind) {
  case T_NUM:
    NEXT();
    return (expr_t){.kind = E_NUM, .num = next.num};
  case T_STR:
    NEXT();
    return (expr_t){.kind = E_STR, .str = next.str};
  case T_IDENT:
    NEXT();
    return (expr_t){.kind = E_VAR, .var = (evar_t){.name = next.ident}};
  case T_LPAR: {
    NEXT();
    if (PEEK().kind == T_RPAR) {
      NEXT();
      return (expr_t){.kind = E_UNIT};
    }
    expr_t e = CALL(expr);
    ASSERT(e, STR("invalid parenthesized expression"));
    if (PEEK().kind == T_RPAR) {
      NEXT();
      return e;
    } else {
      return ERROR("unbalanced parenthesis");
    }
    break;
  }
  case T_TRUE:
    NEXT();
    return (expr_t){.kind = E_BOOL, .boolean = true};
  case T_FALSE:
    NEXT();
    return (expr_t){.kind = E_BOOL, .boolean = false};
  default:
    return NOMATCH();
  }
}
END_PARSER()

PARSER(funcall) {
  expr_t callee = CALL(atom);
  ASSERT(callee, STR("invalid callee expression"));
  if (callee.kind == E_NOMATCH) {
    return ERROR("invalid callee expression");
  }
  loop {
    expr_t param = CALL(atom);
    ASSERT(param, STR("invalid param expression"));
    if (param.kind == E_NOMATCH) {
      return callee;
    } else {
      expr_t *f = gcalloc(sizeof(expr_t));
      expr_t *x = gcalloc(sizeof(expr_t));
      *f = callee;
      *x = param;
      callee =
          (expr_t){.kind = E_CALL, .call = (ecall_t){.callee = f, .param = x}};
    }
  }
}
END_PARSER()

PARSER(unary) {
  token_t sign = PEEK();
  if (sign.kind == T_MINUS) {
    NEXT();
  }
  expr_t expr = CALL(funcall);
  if (sign.kind == T_MINUS) {
    ASSERT(expr, STR("invalid negation parameter"));
    expr_t *e = gcalloc(sizeof(expr_t));
    *e = expr;
    return (expr_t){.kind = E_NEG, .unop = (eunop_t){.rhs = e}};
  } else {
    return expr;
  }
}
END_PARSER()

PARSER(product) {
  expr_t lhs = CALL(unary);
  loop {
    token_t sign = PEEK();
    if (sign.kind == T_STAR || sign.kind == T_SLASH) {
      NEXT();
      expr_t rhs = CALL(unary);
      ASSERT(rhs, STR("invalid product expression"));
      expr_t *l = gcalloc(sizeof(expr_t));
      expr_t *r = gcalloc(sizeof(expr_t));
      *l = lhs;
      *r = rhs;
      exprkind_t kind = sign.kind == T_STAR ? E_MUL : E_DIV;
      lhs = (expr_t){.kind = kind, .binop = (ebinop_t){.lhs = l, .rhs = r}};
    } else {
      return lhs;
    }
  }
}
END_PARSER()

PARSER(addition) {
  expr_t lhs = CALL(product);
  loop {
    token_t sign = PEEK();
    if (sign.kind == T_PLUS || sign.kind == T_MINUS) {
      NEXT();
      expr_t rhs = CALL(product);
      ASSERT(rhs, STR("invalid addition expression"));
      expr_t *l = gcalloc(sizeof(expr_t));
      expr_t *r = gcalloc(sizeof(expr_t));
      *l = lhs;
      *r = rhs;
      exprkind_t kind = sign.kind == T_PLUS ? E_ADD : E_SUB;
      lhs = (expr_t){.kind = kind, .binop = (ebinop_t){.lhs = l, .rhs = r}};
    } else {
      return lhs;
    }
  }
}
END_PARSER()

PARSER(equality) {
  expr_t lhs = CALL(addition);
  token_t sign = PEEK();
  if (sign.kind == T_EQEQ) {
    NEXT();
    expr_t rhs = CALL(addition);
    ASSERT(rhs, STR("invalid equality expression"));
    expr_t *l = gcalloc(sizeof(expr_t));
    expr_t *r = gcalloc(sizeof(expr_t));
    *l = lhs;
    *r = rhs;
    return (expr_t){.kind = E_EQ, .binop = (ebinop_t){.lhs = l, .rhs = r}};
  } else {
    return lhs;
  }
}
END_PARSER()

PARSER(branch) {
  if (PEEK().kind != T_IF) {
    return NOMATCH();
  } else {
    NEXT();
  }
  expr_t cond = CALL(expr);
  ASSERT(cond, STR("invalid condition expression"));
  if (NEXT().kind != T_THEN) {
    return ERROR("expected then keyword");
  }
  expr_t then_body = CALL(expr);
  ASSERT(then_body, STR("invalid if-then body"));
  if (NEXT().kind != T_ELSE) {
    return ERROR("expected else keyword");
  }
  expr_t else_body = CALL(expr);
  ASSERT(else_body, STR("invalid if-then-else body"));

  expr_t *c = gcalloc(sizeof(expr_t));
  expr_t *t = gcalloc(sizeof(expr_t));
  expr_t *e = gcalloc(sizeof(expr_t));
  *c = cond;
  *t = then_body;
  *e = else_body;
  return (expr_t){.kind = E_IFTHEN,
                  .ifthen = (eif_t){.cond = c, .then_body = t, .else_body = e}};
}
END_PARSER()

PARSER(fun) {
  if (PEEK().kind != T_FUN) {
    return NOMATCH();
  } else {
    NEXT();
  }
  token_t param = NEXT();
  if (param.kind != T_IDENT) {
    return ERROR("expected param identifier");
  }
  if (NEXT().kind != T_ARROW) {
    return ERROR("expected functional arrow");
  }
  expr_t body = CALL(expr);
  ASSERT(body, STR("invalid function body"));

  expr_t *b = gcalloc(sizeof(expr_t));
  *b = body;
  return (expr_t){.kind = E_FUN,
                  .fun = (efun_t){.param = param.ident, .body = b}};
}
END_PARSER()

PARSER(binding) {
  if (PEEK().kind != T_LET) {
    return NOMATCH();
  } else {
    NEXT();
  }
  bool rec = (PEEK().kind == T_REC);
  if (rec) {
    NEXT();
  }
  token_t name = NEXT();
  if (name.kind != T_IDENT) {
    return ERROR("expected binding identifier");
  }
  tokenbuf_t params = tokenbuf_new();
  while (PEEK().kind == T_IDENT) {
    tokenbuf_push(&params, NEXT());
  }
  if (NEXT().kind != T_EQ) {
    return ERROR("expected equal sign");
  }
  expr_t value = CALL(expr);
  ASSERT(value, STR("invalid binding value expression"));
  if (NEXT().kind != T_IN) {
    return ERROR("expected 'in' keyword");
  }
  expr_t body = CALL(expr);
  ASSERT(body, STR("invalid binding body expression"));

  for (usize j = 0; j < params.len; ++j) {
    usize i = params.len - 1 - j;
    expr_t *v = gcalloc(sizeof(expr_t));
    *v = value;
    str_t p = params.tokens[i].ident;
    value = (expr_t){.kind = E_FUN, .fun = (efun_t){.param = p, .body = v}};
  }

  expr_t *v = gcalloc(sizeof(expr_t));
  expr_t *b = gcalloc(sizeof(expr_t));
  *v = value;
  *b = body;

  exprkind_t kind = rec ? E_LETREC : E_LET;
  return (expr_t){.kind = kind,
                  .let = (elet_t){.name = name.ident, .expr = v, .body = b}};
}
END_PARSER()

PARSER(expr) {
  expr_t res;
  res = CALL(branch);
  if (res.kind != E_NOMATCH) {
    return res;
  }
  res = CALL(fun);
  if (res.kind != E_NOMATCH) {
    return res;
  }
  res = CALL(binding);
  if (res.kind != E_NOMATCH) {
    return res;
  }
  return CALL(equality);
}
END_PARSER()

#undef ERROR
#define ERROR(__msg)                                                           \
  ((toplevel_t){.kind = TL_ERROR,                                              \
                .error = (error_chain_t){.msg = STR(__msg), .next = NULL}})

#undef ASSERT
#define ASSERT(__expr, __msg)                                                  \
  {                                                                            \
    if (__expr.kind == E_ERROR) {                                              \
      error_chain_t *next = gcalloc(sizeof(error_chain_t));                    \
      *next = __expr.error;                                                    \
      error_chain_t error = (error_chain_t){.msg = __msg, .next = next};       \
      return (toplevel_t){.kind = TL_ERROR, .error = error};                   \
    }                                                                          \
  }

toplevel_t toplevel(parser_t *__parser) {
  if (PEEK().kind != T_LET) {
    expr_t e = CALL(expr);
    ASSERT(e, STR("invalid toplevel expression"));
    if (PEEK().kind == T_SEMISEMI) {
      NEXT();
    }
    return (toplevel_t){.kind = TL_EXPR, .expr = e};
  } else {
    NEXT();
  }
  bool rec = (PEEK().kind == T_REC);
  if (rec) {
    NEXT();
  }

  token_t name = NEXT();
  if (name.kind != T_IDENT) {
    return ERROR("expected binding identifier");
  }

  tokenbuf_t params = tokenbuf_new();
  while (PEEK().kind == T_IDENT) {
    tokenbuf_push(&params, NEXT());
  }
  if (PEEK().kind != T_EQ) {
    token_t next = PEEK();
    fprint_token(stderr, &next);
    return ERROR("expected equal sign");
  }
  NEXT();
  expr_t value = CALL(expr);
  ASSERT(value, STR("invalid binding value expression"));

  for (usize j = 0; j < params.len; ++j) {
    usize i = params.len - 1 - j;
    expr_t *v = gcalloc(sizeof(expr_t));
    *v = value;
    str_t p = params.tokens[i].ident;
    value = (expr_t){.kind = E_FUN, .fun = (efun_t){.param = p, .body = v}};
  }

  if (PEEK().kind == T_IN) {
    NEXT();
    expr_t body = CALL(expr);
    ASSERT(body, STR("invalid binding body expression"));
    if (PEEK().kind == T_SEMISEMI) {
      NEXT();
    }
    expr_t *v = gcalloc(sizeof(expr_t));
    expr_t *b = gcalloc(sizeof(expr_t));
    *v = value;
    *b = body;
    exprkind_t kind = (rec) ? E_LETREC : E_LET;
    expr_t let =
        (expr_t){.kind = kind,
                 .let = (elet_t){.name = name.ident, .expr = v, .body = b}};
    return (toplevel_t){.kind = TL_EXPR, .expr = let};
  }

  if (PEEK().kind == T_SEMISEMI) {
    NEXT();
  }

  toplevelkind_t kind = rec ? TL_LETREC : TL_LET;
  return (toplevel_t){
      .kind = kind, .let = (toplevel_let_t){.name = name.ident, .expr = value}};
}

static void _fprint_expr(FILE *f, expr_t *e, usize level) {
  switch (e->kind) {
  case E_ERROR:
    fprintf(f, "<ERROR (%.*s)>", (int)(e->error.msg.len), e->error.msg.data);
    break;
  case E_NOMATCH:
    fprintf(f, "<NO MATCH>");
    break;
  case E_NUM:
    fprintf(f, "%.*g", 16, e->num);
    break;
  case E_STR:
    fdebug_str(f, e->str.data, e->str.len);
    break;
  case E_BOOL:
    if (e->boolean) {
      fprintf(f, "true");
    } else {
      fprintf(f, "false");
    }
    break;
  case E_UNIT:
    fprintf(f, "()");
    break;
  case E_VAR:
    fprintf(f, "%.*s", (int)(e->var.name.len), e->var.name.data);
    break;
  case E_CALL:
    fprintf(f, "(");
    _fprint_expr(f, e->call.callee, level);
    fprintf(f, ") (");
    _fprint_expr(f, e->call.param, level);
    fprintf(f, ")");
    break;
  case E_LET:
  case E_LETREC:
    fprintf(f, "let ");
    if (e->kind == E_LETREC) {
      fprintf(f, "rec ");
    }
    fprintf(f, "%.*s =\n", (int)(e->let.name.len), e->let.name.data);
    for (usize i = 0; i <= level; ++i) {
      fprintf(f, "  ");
    }
    _fprint_expr(f, e->let.expr, level + 1);
    fprintf(f, "\n");
    for (usize i = 0; i < level; ++i) {
      fprintf(f, "  ");
    }
    fprintf(f, "in\n");
    for (usize i = 0; i <= level; ++i) {
      fprintf(f, "  ");
    }
    _fprint_expr(f, e->let.body, level + 1);
    break;
  case E_FUN:
    fprintf(f, "fun %.*s ->\n", (int)(e->fun.param.len), e->fun.param.data);
    for (usize i = 0; i <= level; ++i) {
      fprintf(f, "  ");
    }
    _fprint_expr(f, e->fun.body, level + 1);
    break;
  case E_IFTHEN:
    fprintf(f, "if (");
    _fprint_expr(f, e->ifthen.cond, level);
    fprintf(f, ") then\n");
    for (usize i = 0; i <= level; ++i) {
      fprintf(f, "  ");
    }
    _fprint_expr(f, e->ifthen.then_body, level + 1);
    fprintf(f, "\n");
    for (usize i = 0; i < level; ++i) {
      fprintf(f, "  ");
    }
    fprintf(f, "else\n");
    for (usize i = 0; i <= level; ++i) {
      fprintf(f, "  ");
    }
    _fprint_expr(f, e->ifthen.else_body, level + 1);
    break;
  case E_NEG:
    fprintf(f, "-(");
    _fprint_expr(f, e->unop.rhs, level);
    fprintf(f, ")");
    break;
  case E_ADD:
  case E_SUB:
  case E_MUL:
  case E_DIV:
  case E_EQ: {
    const char *op = "";
    switch (e->kind) {
    case E_ADD:
      op = "+";
      break;
    case E_SUB:
      op = "-";
      break;
    case E_MUL:
      op = "*";
      break;
    case E_DIV:
      op = "/";
      break;
    case E_EQ:
      op = "==";
      break;
    default:
      break;
    }
    fprintf(f, "(");
    _fprint_expr(f, e->binop.lhs, level);
    fprintf(f, ") %s (", op);
    _fprint_expr(f, e->binop.rhs, level);
    fprintf(f, ")");
    break;
  }
  }
}

void fprint_expr(FILE *f, expr_t *e) { _fprint_expr(f, e, 0); }

void fprint_toplevel(FILE *f, toplevel_t *tl) {
  switch (tl->kind) {
  case TL_ERROR:
    fprintf(f, "<ERROR (%.*s)>", (int)(tl->error.msg.len), tl->error.msg.data);
    break;
  case TL_LET:
  case TL_LETREC:
    fprintf(f, "let ");
    if (tl->kind == TL_LETREC) {
      fprintf(f, "rec ");
    }
    fprintf(f, "%.*s =\n  ", (int)(tl->let.name.len), tl->let.name.data);
    _fprint_expr(f, &tl->let.expr, 1);
    break;
  case TL_EXPR:
    _fprint_expr(f, &tl->expr, 0);
    break;
  }
}
