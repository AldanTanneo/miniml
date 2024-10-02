#include <gc/gc.h>
#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "eval.h"
#include "lex.h"
#include "utils.h"

#define BUFFER_WINDOW (1024ul)

void run(FILE *file) {
  bytes_t bytes = bytes_new();
  bytes_reserve(&bytes, BUFFER_WINDOW);

  lexstream_t stream;
  token_t tok;
  tokenbuf_t tokens = tokenbuf_new();
  usize read;

  loop {
    read = bytes_fread(&bytes, file);
    if (read == 0) {
      bytes_push(&bytes, '\n');
    }
    stream = lexstream_new(bytes.data, bytes.len);

    loop {
      tok = lex(&stream);

      if (tok.kind == T_INCOMPLETE) {
        if (read == 0 && stream.seen != 0) {
          fprintf(stderr, "\nincomplete token: ");
          fdebug_str(stderr, stream.data, stream.seen);
          eprintln("");
          panic("unfinished token in input stream");
        }
        break;
      }

      if (tok.kind == T_ERROR) {
        fprintf(stderr, "\ninvalid token: ");
        fdebug_str(stderr, stream.data, stream.seen);
        eprintln("");
        panic("malformed token in input stream: %s", tok.error);
      }
      tokenbuf_push(&tokens, tok);
    }

    if (read == 0 && tok.kind == T_INCOMPLETE) {
      break;
    }
    bytes_clear_start(&bytes, (usize)(stream.data - bytes.data));
    if (tok.kind == T_INCOMPLETE) {
      bytes_reserve(&bytes, BUFFER_WINDOW);
    }
  }

  if (file != stdin) {
    fclose(file);
  }

  parser_t parser = parser_new(tokens);

  toplevel_t tl;
  env_t env = NULL;
  do {
    tl = toplevel(&parser);
    env = walk_file(env, &tl);
  } while (parser.pos < parser.len && tl.kind != TL_ERROR);

  if (tl.kind == TL_ERROR) {
    error_chain_t error = tl.error;
    while (error.next != NULL) {
      error = *error.next;
      println("%.*s", (int)(error.msg.len), error.msg.data);
    }
  }
}

i32 main(i32 argc, char *argv[]) {
  FILE *file;
  if (argc < 2) {
    file = stdin;
  } else {
    file = fopen(argv[1], "r");
    if (file == NULL) {
      panic("failed to open file!");
    }
  }

  GC_INIT();

  run(file);

  print_memory_use();
}
