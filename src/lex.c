#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

#include "interner.h"
#include "lex.h"
#include "utils.h"

#define LEXER(NAME, args...)                                                   \
  token_t NAME(lexstream_t *__stream, ##args) {                                \
    goto __start;                                                              \
  __start:

#define END_LEXER() }

#define STREAM() (__stream)
#define PEEK() lex_peek(__stream)
#define NEXT() lex_next(__stream)
#define CONSUME() lex_consume(__stream)
#define LAST() lex_last(__stream)
#define REWIND(n) lex_rewind(__stream, n)
#define RESET() lex_reset(__stream)

#define LEX()                                                                  \
  {                                                                            \
    CONSUME();                                                                 \
    goto __start;                                                              \
  }

#define CALL(_lexer, args...)                                                  \
  { return (_lexer)(__stream, ##args); }

#define TOK(KIND)                                                              \
  {                                                                            \
    CONSUME();                                                                 \
    return mktoken(KIND);                                                      \
  }

#define NUM(n)                                                                 \
  {                                                                            \
    CONSUME();                                                                 \
    return mktoken_num(n);                                                     \
  }

#define STRING(data, len)                                                      \
  {                                                                            \
    CONSUME();                                                                 \
    return mktoken_str(data, len);                                             \
  }

#define IDENT()                                                                \
  {                                                                            \
    if (__stream->seen != 0) {                                                 \
      return mktoken_ident(__stream);                                          \
    } else {                                                                   \
      return lex_error("invalid identifier");                                  \
    }                                                                          \
  }

#define ERROR(MSG)                                                             \
  { return lex_error(MSG); }

#define INCOMPLETE()                                                           \
  { return mktoken(T_INCOMPLETE); }

#define TRY(_lexer, args...)                                                   \
  {                                                                            \
    token_t _try = (_lexer)(__stream, ##args);                                 \
    switch (_try.kind) {                                                       \
    case T_ERROR:                                                              \
      break;                                                                   \
    case T_INCOMPLETE:                                                         \
      INCOMPLETE();                                                            \
    default:                                                                   \
      return _try;                                                             \
    }                                                                          \
  }

#define MATCH(chr)                                                             \
  {                                                                            \
    i16 _chr = (chr);                                                          \
    i16 _next = NEXT();                                                        \
    if (_next == EOF) {                                                        \
      INCOMPLETE();                                                            \
    } else if (_next != _chr) {                                                \
      str_t msg =                                                              \
          str_new("unexpected character (wanted " #chr ", found '\\'))')");    \
      if (_next == '\n' || _next == '\r' || _next == '\t' || _next == '\0' ||  \
          _next == '\\' || _next == '\'') {                                    \
        msg.data[msg.len - 5] =                                                \
            (_next == '\n') * 'n' + (_next == '\r') * 'r' +                    \
            (_next == '\t') * 't' + (_next == '\0') * '0' +                    \
            (_next == '\\') * '\\' + (_next == '\'') * '\'';                   \
        msg.data[msg.len - 4] = '\'';                                          \
        msg.data[msg.len - 2] = '\0';                                          \
      } else if (isprint(_next)) {                                             \
        msg.data[msg.len - 6] = (u8)_next;                                     \
        msg.data[msg.len - 3] = '\0';                                          \
      } else {                                                                 \
        msg.data[msg.len - 5] = 'x';                                           \
        msg.data[msg.len - 4] = hexdigit((u8)_next / 16);                      \
        msg.data[msg.len - 3] = hexdigit((u8)_next % 16);                      \
      }                                                                        \
      ERROR((char *)msg.data);                                                 \
    }                                                                          \
  }

static inline u8 hexdigit(u8 x) {
  if (x < 16) {
    return (u8)("0123456789abcdef"[x]);
  } else {
    return 0;
  }
}

static i16 lex_next(lexstream_t *stream) {
  if (stream->seen < stream->len) {
    u8 res = stream->data[stream->seen];
    stream->seen += 1;
    return res;
  } else {
    return EOF;
  }
}

static i16 lex_peek(lexstream_t *stream) {
  if (stream->seen < stream->len) {
    u8 res = stream->data[stream->seen];
    return res;
  } else {
    return EOF;
  }
}

static void lex_consume(lexstream_t *stream) {
  stream->data += stream->seen;
  stream->len -= stream->seen;
  stream->seen = 0;
}

static i16 lex_last(lexstream_t *stream) {
  if (stream->seen > 0) {
    return stream->data[stream->seen - 1];
  } else {
    return EOF;
  }
}

static void lex_rewind(lexstream_t *stream, usize n) {
  if (n < stream->seen) {
    stream->seen -= n;
  } else {
    stream->seen = 0;
  }
}

static token_t lex_error(char *msg) {
  return (token_t){.kind = T_ERROR, .error = msg};
}

static token_t mktoken(tkind_t kind) {
  switch (kind) {
  case T_NUM:
    return (token_t){.kind = T_NUM, .num = 0.0};
    break;
  case T_STR:
    return (token_t){.kind = T_STR, .str = intern(str_empty())};
  default:
    return (token_t){.kind = kind};
  }
}

static token_t mktoken_num(f64 num) {
  return (token_t){.kind = T_NUM, .num = num};
}

static token_t mktoken_str(u8 *data, usize len) {
  str_t str = str_make(data, len);
  return (token_t){.kind = T_STR, .str = intern(str)};
}

static token_t mktoken_ident(lexstream_t *stream) {
  str_t ident = str_from(stream->data, stream->seen);
  lex_consume(stream);
  return (token_t){.kind = T_IDENT, .ident = intern(ident)};
}

static LEXER(ident) {
  i16 peeked;
  loop {
    peeked = PEEK();
    if (peeked == EOF) {
      INCOMPLETE();
    } else if ((peeked >= 'a' && peeked <= 'z') ||
               (peeked >= 'A' && peeked <= 'Z') ||
               (peeked >= '0' && peeked <= '9') || peeked == '_') {
      NEXT();
    } else {
      IDENT();
    }
  }
}
END_LEXER()

static LEXER(keyword, tkind_t kind, const char *keyword) {
  i16 peeked;
  usize i;
  for (i = 0; keyword[i] != 0; ++i) {
    peeked = PEEK();
    if (peeked == EOF) {
      INCOMPLETE();
    } else if (peeked == keyword[i]) {
      NEXT();
    } else {
      REWIND(i);
      ERROR("no match");
    }
  }
  peeked = PEEK();
  if (peeked == EOF) {
    INCOMPLETE();
  } else if ((peeked >= 'a' && peeked <= 'z') ||
             (peeked >= 'A' && peeked <= 'Z') ||
             (peeked >= '0' && peeked <= '9') || peeked == '_') {
    REWIND(i);
    ERROR("no match");
  } else {
    TOK(kind);
  }
}
END_LEXER()

static LEXER(string) {
  bytes_t bytes = bytes_new();
  i16 cur;
  loop {
    cur = NEXT();
    switch (cur) {
    case EOF:
      INCOMPLETE();
    case '"':
      bytes_shrink(&bytes);
      STRING(bytes.data, bytes.len);
    case '\\':
      cur = NEXT();
      switch (cur) {
      case EOF:
        INCOMPLETE();
      case '"':
      case '\\':
        bytes_push(&bytes, (u8)cur);
        break;
      case '0':
        bytes_push(&bytes, '\0');
        break;
      case 'n':
        bytes_push(&bytes, '\n');
        break;
      case 't':
        bytes_push(&bytes, '\t');
        break;
      case 'r':
        bytes_push(&bytes, '\r');
        break;
      case 'x': {
        u8 val = 0;
        i16 cur;
        for (usize i = 0; i < 2; ++i) {
          cur = NEXT();
          val *= 16;
          if (cur == EOF) {
            INCOMPLETE();
          } else if (cur >= '0' && cur <= '9') {
            val += (u8)cur - '0';
          } else if (cur >= 'a' && cur <= 'f') {
            val += (u8)cur - 'a' + 10;
          } else if (cur >= 'A' && cur <= 'F') {
            val += (u8)cur - 'F' + 10;
          } else {
            ERROR("invalid hex digit in hex escape");
          }
        }
        if (val > 0x7f) {
          ERROR("invalid hex escape (value is above 0x7f)");
        }
        bytes_push(&bytes, val);
        break;
      }
      case 'u': {
        MATCH('{');
        u32 val = 0;
        i16 cur;
        for (usize i = 0; i < 6; ++i) {
          cur = NEXT();
          if (cur == EOF) {
            INCOMPLETE();
          } else if (cur == '}') {
            if (i == 0) {
              ERROR("invalid unicode escape (no digits)");
            }
            REWIND(1);
            break;
          }

          val *= 16;

          if (cur >= '0' && cur <= '9') {
            val += (u8)cur - '0';
          } else if (cur >= 'a' && cur <= 'f') {
            val += (u8)cur - 'a' + 10;
          } else if (cur >= 'A' && cur <= 'F') {
            val += (u8)cur - 'A' + 10;
          } else {
            ERROR("invalid hex digit in unicode escape");
          }
        }
        MATCH('}');
        if (val < 0x80) {
          bytes_push(&bytes, (u8)val);
        } else if (val < 0x800) {
          u8 b[2] = {0b110 << 5, 0b10 << 6};
          b[0] |= (val >> 6) & 0b11111;
          b[1] |= (val >> 0) & 0b111111;
          bytes_push(&bytes, b[0]);
          bytes_push(&bytes, b[1]);
        } else if (val < 0x10000) {
          u8 b[3] = {0b1110 << 4, 0b10 << 6, 0b10 << 6};
          b[0] |= (val >> 12) & 0b1111;
          b[1] |= (val >> 6) & 0b111111;
          b[2] |= (val >> 0) & 0b111111;
          bytes_push(&bytes, b[0]);
          bytes_push(&bytes, b[1]);
          bytes_push(&bytes, b[2]);
        } else if (val <= 0x10FFFF) {
          u8 b[4] = {0b11110 << 3, 0b10 << 6, 0b10 << 6, 0b10 << 6};
          b[0] |= (val >> 18) & 0b111;
          b[1] |= (val >> 12) & 0b111111;
          b[2] |= (val >> 6) & 0b111111;
          b[3] |= (val >> 0) & 0b111111;
          bytes_push(&bytes, b[0]);
          bytes_push(&bytes, b[1]);
          bytes_push(&bytes, b[2]);
          bytes_push(&bytes, b[3]);
        } else {
          ERROR("unicode escape must be at most 10FFFF");
        }
        break;
      }
      default:
        ERROR("invalid escape in string literal");
      }
      break;
    default:
      bytes_push(&bytes, (u8)cur);
    }
  }
}
END_LEXER()

LEXER(lex) {
  i16 next = NEXT();
  switch (next) {
  case EOF:
    INCOMPLETE();
  case ' ':
  case '\t':
  case '\n':
  case '\r':
    LEX();
  case '(':
    TOK(T_LPAR);
  case ')':
    TOK(T_RPAR);
  case '+':
    TOK(T_PLUS);
  case '*':
    TOK(T_STAR);
  case '-':
    switch (PEEK()) {
    case EOF:
      INCOMPLETE();
    case '>':
      NEXT();
      TOK(T_ARROW);
    default:
      TOK(T_MINUS);
    }
  case '=':
    switch (PEEK()) {
    case EOF:
      INCOMPLETE();
    case '=':
      NEXT();
      TOK(T_EQEQ);
    default:
      TOK(T_EQ);
    }
  case ';':
    switch (PEEK()) {
    case EOF:
      INCOMPLETE();
    case ';':
      NEXT();
      TOK(T_SEMISEMI);
    default:
      TOK(T_SEMI);
    }
  case '/':
    switch (PEEK()) {
    case EOF:
      INCOMPLETE();
    case '/': {
      NEXT();
      i16 cur;
      do {
        cur = NEXT();
      } while (cur != '\n' && cur != EOF);
      LEX();
    }
    case '*': {
      panic("todo: block comments");
    }
    default:
      TOK(T_SLASH);
    }
  case '"': {
    CALL(string);
  }
  case '0' ... '9': {
    long double n = LAST() - '0';
    i16 peeked;
    long double decimals = 0;
    loop {
      peeked = PEEK();
      if (peeked == EOF) {
        INCOMPLETE();
      } else if (peeked >= '0' && peeked <= '9') {
        NEXT();
        n *= 10;
        n += peeked - '0';
        decimals /= 10;
      } else if (peeked == '_') {
        NEXT();
      } else if (peeked == '.') {
        NEXT();
        decimals = 1;
      } else {
        decimals += decimals == 0;
        NUM((f64)(n * decimals));
      }
    }
  }
  default:
    REWIND(1); // rewind by 1 to match on the previous
               // character in the sub lexers
    TRY(keyword, T_IN, "in");
    TRY(keyword, T_IF, "if");
    TRY(keyword, T_LET, "let");
    TRY(keyword, T_FUN, "fun");
    TRY(keyword, T_REC, "rec");
    TRY(keyword, T_THEN, "then");
    TRY(keyword, T_ELSE, "else");
    TRY(keyword, T_TRUE, "true");
    TRY(keyword, T_FALSE, "false");
    TRY(ident);
    NEXT(); // no match; advance again
    ERROR("invalid character");
  }
}
END_LEXER()

lexstream_t lexstream_new(const u8 *data, usize len) {
  return (lexstream_t){.data = data, .len = len, .seen = 0};
}

void fprint_token(FILE *f, token_t *tok) {
  switch (tok->kind) {
  case T_ERROR:
    fprintf(f, "ERROR(%s)", tok->error);
    break;
  case T_INCOMPLETE:
    fprintf(f, "EOF");
    break;
  case T_NUM:
    fprintf(f, "\x1b[33m%.*g\x1b[0m", 16, tok->num);
    break;
  case T_STR:
    fdebug_str_color(f, tok->str.data, tok->str.len);
    break;
  case T_IDENT:
    fprintf(f, "\x1b[31m%.*s\x1b[0m", (i32)(tok->ident.len), tok->ident.data);
    break;
  case T_LET:
    fprintf(f, "\x1b[035mlet\x1b[0m");
    break;
  case T_REC:
    fprintf(f, "\x1b[035mrec\x1b[0m");
    break;
  case T_FUN:
    fprintf(f, "\x1b[035mfun\x1b[0m");
    break;
  case T_IN:
    fprintf(f, "\x1b[035min\x1b[0m");
    break;
  case T_IF:
    fprintf(f, "\x1b[035mif\x1b[0m");
    break;
  case T_THEN:
    fprintf(f, "\x1b[035mthen\x1b[0m");
    break;
  case T_ELSE:
    fprintf(f, "\x1b[035melse\x1b[0m");
    break;
  case T_TRUE:
    fprintf(f, "\x1b[33mtrue\x1b[0m");
    break;
  case T_FALSE:
    fprintf(f, "\x1b[33mfalse\x1b[0m");
    break;
  case T_LPAR:
    fprintf(f, "\x1b[034m(\x1b[0m");
    break;
  case T_RPAR:
    fprintf(f, "\x1b[034m)\x1b[0m");
    break;
  case T_PLUS:
    fprintf(f, "+");
    break;
  case T_MINUS:
    fprintf(f, "-");
    break;
  case T_STAR:
    fprintf(f, "*");
    break;
  case T_SLASH:
    fprintf(f, "/");
    break;
  case T_EQ:
    fprintf(f, "=");
    break;
  case T_EQEQ:
    fprintf(f, "==");
    break;
  case T_SEMI:
    fprintf(f, ";");
    break;
  case T_ARROW:
    fprintf(f, "->");
    break;
  case T_SEMISEMI:
    fprintf(f, ";;");
    break;
  }
}

tokenbuf_t tokenbuf_new() {
  return (tokenbuf_t){.tokens = NULL, .len = 0, .cap = 0};
}

void tokenbuf_push(tokenbuf_t *tokens, token_t tok) {
  if (tokens->len >= tokens->cap) {
    usize new_cap = (tokens->len == 0) ? 4 : (tokens->len * 2);
    token_t *data = gcrealloc(tokens->tokens, new_cap * sizeof(token_t));
    tokens->cap = new_cap;
    tokens->tokens = data;
  }
  tokens->tokens[tokens->len] = tok;
  tokens->len += 1;
}
