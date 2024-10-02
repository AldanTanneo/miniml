#include <stdio.h>
#include <string.h>

#include "eval.h"
#include "utils.h"

#define ERROR(__msg) ((value_t){.kind = V_ERROR, .error = STR(__msg)})
#define BUBBLE(__x)                                                            \
  {                                                                            \
    if (__x.kind == V_ERROR) {                                                 \
      return __x;                                                              \
    }                                                                          \
  }
#define EVAL(__bind, __args...)                                                \
  value_t __bind = eval_expr(__args);                                          \
  BUBBLE(__bind);

value_t *find_env(env_t env, str_t name) {
  while (env != NULL && !str_comp(env->name, name)) {
    env = env->next;
  }
  if (env == NULL) {
    return NULL;
  } else {
    return &env->value;
  }
}

env_t push_env(env_t env, str_t name, value_t value) {
  env_t newenv = gcalloc(sizeof(*env));
  newenv->name = name;
  newenv->value = value;
  newenv->next = env;
  return newenv;
}

value_t eval_expr(env_t env, expr_t *expr) {
__start:
  switch (expr->kind) {
  case E_NUM:
    return (value_t){.kind = V_NUM, .num = expr->num};
  case E_STR:
    return (value_t){.kind = V_STR, .str = expr->str};
  case E_BOOL:
    return (value_t){.kind = V_BOOL, .boolean = expr->boolean};
  case E_UNIT:
    return (value_t){.kind = V_UNIT};
  case E_VAR: {
    value_t *val = find_env(env, expr->var.name);
    if (val == NULL) {
      return ERROR("unknown binding");
    } else {
      return *val;
    }
  }
  case E_CALL: {
    EVAL(callee, env, expr->call.callee);
    EVAL(param, env, expr->call.param);
    if (callee.kind != V_FUN) {
      return ERROR("trying to call non function");
    } else {
      env = push_env(callee.fun->env, callee.fun->param, param);
      expr = callee.fun->expr;
      goto __start;
    }
  }
  case E_LET: {
    EVAL(value, env, expr->let.expr);
    env = push_env(env, expr->let.name, value);
    expr = expr->let.body;
    goto __start;
  }
  case E_LETREC: {
    EVAL(fun, env, expr->let.expr);
    if (fun.kind != V_FUN) {
      return ERROR("let rec binding can only be used with a function");
    } else {
      fun.fun->env = push_env(fun.fun->env, expr->let.name, fun);
      env = push_env(env, expr->let.name, fun);
      expr = expr->let.body;
      goto __start;
    }
  }
  case E_FUN: {
    vfun_t *fun = gcalloc(sizeof(vfun_t));
    fun->env = env;
    fun->param = expr->fun.param;
    fun->expr = expr->fun.body;
    return (value_t){.kind = V_FUN, .fun = fun};
  }
  case E_IFTHEN: {
    EVAL(cond, env, expr->ifthen.cond);
    if (cond.kind != V_BOOL) {
      return ERROR("condition is not a boolean");
    } else if (cond.boolean) {
      expr = expr->ifthen.then_body;
    } else {
      expr = expr->ifthen.else_body;
    }
    goto __start;
  }
  case E_NEG: {
    EVAL(rhs, env, expr->unop.rhs);
    if (rhs.kind == V_NUM) {
      return (value_t){.kind = V_NUM, .num = -rhs.num};
    } else if (rhs.kind == V_BOOL) {
      return rhs;
    } else {
      return ERROR("negation operand is not a number");
    }
  }
  case E_ADD: {
    EVAL(lhs, env, expr->binop.lhs);
    EVAL(rhs, env, expr->binop.rhs);
    if (lhs.kind != rhs.kind) {
      return ERROR("incompatible types in addition");
    }
    switch (lhs.kind) {
    case V_NUM:
      return (value_t){.kind = V_NUM, .num = lhs.num + rhs.num};
    case V_BOOL:
      return (value_t){.kind = V_BOOL, .boolean = lhs.boolean ^ rhs.boolean};
    case V_STR: {
      bytes_t bytes = bytes_new();
      bytes_reserve(&bytes, lhs.str.len + rhs.str.len);
      memcpy(&bytes.data[0], lhs.str.data, lhs.str.len);
      memcpy(&bytes.data[lhs.str.len], rhs.str.data, rhs.str.len);

      str_t res = str_make(bytes.data, lhs.str.len + rhs.str.len);
      return (value_t){.kind = V_STR, .str = res};
    }
    default:
      return ERROR("cannot add this type");
    }
  }
  case E_SUB: {
    EVAL(lhs, env, expr->binop.lhs);
    EVAL(rhs, env, expr->binop.rhs);
    if (lhs.kind != rhs.kind) {
      return ERROR("incompatible types in substraction");
    }
    switch (lhs.kind) {
    case V_NUM:
      return (value_t){.kind = V_NUM, .num = lhs.num - rhs.num};
    case V_BOOL:
      return (value_t){.kind = V_BOOL, .boolean = lhs.boolean ^ rhs.boolean};
    default:
      return ERROR("cannot substract this type");
    }
  }
  case E_MUL: {
    EVAL(lhs, env, expr->binop.lhs);
    EVAL(rhs, env, expr->binop.rhs);
    if (lhs.kind != rhs.kind) {
      return ERROR("incompatible types in multiplication");
    }
    switch (lhs.kind) {
    case V_NUM:
      return (value_t){.kind = V_NUM, .num = lhs.num * rhs.num};
    case V_BOOL:
      return (value_t){.kind = V_BOOL, .boolean = lhs.boolean & rhs.boolean};
    default:
      return ERROR("cannot substract this type");
    }
  }
  case E_DIV: {
    EVAL(lhs, env, expr->binop.lhs);
    EVAL(rhs, env, expr->binop.rhs);
    if (lhs.kind != rhs.kind) {
      return ERROR("incompatible types in division");
    }
    switch (lhs.kind) {
    case V_NUM:
      return (value_t){.kind = V_NUM, .num = lhs.num / rhs.num};
    default:
      return ERROR("cannot divide this type");
    }
  }
  case E_EQ: {
    EVAL(lhs, env, expr->binop.lhs);
    EVAL(rhs, env, expr->binop.rhs);
    if (lhs.kind != rhs.kind) {
      return ERROR("incompatible types in equality");
    }
    switch (lhs.kind) {
    case V_NUM:
      return (value_t){.kind = V_BOOL, .boolean = lhs.num == rhs.num};
    case V_BOOL:
      return (value_t){.kind = V_BOOL, .boolean = lhs.boolean == rhs.boolean};
    case V_STR:
      return (value_t){.kind = V_BOOL, .boolean = str_comp(lhs.str, rhs.str)};
    case V_UNIT:
      return (value_t){.kind = V_BOOL, .boolean = true};
    default:
      return ERROR("cannot compare this type");
    }
  }
  case E_ERROR:
  case E_NOMATCH:
    return ERROR("invalid expression");
  }
  return ERROR("unreachable");
}

env_t walk_file(env_t env, toplevel_t *tl) {
  switch (tl->kind) {
  case TL_EXPR: {
    value_t val = eval_expr(env, &tl->expr);
    if (val.kind != V_UNIT) {
      fprint_value(stdout, &val);
      println("");
    }
    return env;
  }
  case TL_LET: {
    value_t binding = eval_expr(env, &tl->let.expr);
    return push_env(env, tl->let.name, binding);
  }
  case TL_LETREC: {
    value_t binding = eval_expr(env, &tl->let.expr);
    if (binding.kind == V_FUN) {
      binding.fun->env = push_env(binding.fun->env, tl->let.name, binding);
    }
    return push_env(env, tl->let.name, binding);
  }
  case TL_ERROR:
    return env;
  }
  return env;
}

void fprint_value(FILE *f, value_t *val) {
  switch (val->kind) {
  case V_NUM:
    fprintf(f, "%.*g", 16, val->num);
    break;
  case V_STR:
    fdebug_str(f, val->str.data, val->str.len);
    break;
  case V_BOOL:
    if (val->boolean) {
      fprintf(f, "true");
    } else {
      fprintf(f, "false");
    }
    break;
  case V_UNIT:
    break;
  case V_FUN:
    fprintf(f, "<function>");
    break;
  case V_ERROR:
    fprintf(f, "ERROR: %.*s", (int)(val->error.len), val->error.data);
    break;
  }
}
