#pragma once

#include "ast.h"
#include "utils.h"
#include <stdio.h>

typedef struct env *env_t;

typedef enum valuekind {
  V_ERROR = -1,
  V_UNIT,
  V_NUM,
  V_STR,
  V_BOOL,
  V_FUN
} valuekind_t;

typedef struct vfun {
  str_t param;
  expr_t *expr;
  env_t env;
} vfun_t;

typedef struct value {
  valuekind_t kind;
  union {
    str_t error;
    f64 num;
    str_t str;
    bool boolean;
    vfun_t *fun;
  };
} value_t;

struct env {
  str_t name;
  value_t value;
  env_t next;
};

value_t *find_env(env_t env, str_t name);
env_t push_env(env_t env, str_t name, value_t value);

value_t eval_expr(env_t env, expr_t *expr);
env_t walk_file(env_t env, toplevel_t *tl);

void fprint_value(FILE *f, value_t *val);
