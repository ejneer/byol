#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "mpc.h"

/* If we are compiling on Windows compile with these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline function */
char *readline(char *prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

/* Fake add_history function */
void add_history(char *unused) {}

/* Otherwise include the editline headers */
#else

/*
** had to add -ledit compilation flag
** https://stackoverflow.com/a/23313923
*/
#include <editline/readline.h>

#endif

/* create enumeration of possible lval types */
enum { LVAL_NUM, LVAL_ERR };

/* create enumeration of possible error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef struct {
  int type;
  long num;
  int err;
} lval;

/* create a new number type lval */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* create a new error type lval */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

/* print an "lval" */
void lval_print(lval v) {
  switch (v.type) {
  /* in the case the type is a number print it
   * then break out of the switch */
  case LVAL_NUM:
    printf("%li", v.num);
    break;

  case LVAL_ERR:
    if (v.err == LERR_DIV_ZERO) {
      printf("Error: Division by Zero!");
    }
    if (v.err == LERR_BAD_OP) {
      printf("Error: Invalid Operator!");
    }
    if (v.err == LERR_BAD_NUM) {
      printf("Error: Invalid Number!");
    }
    break;
  }
}

void lval_println(lval v) {
  lval_print(v);
  putchar('\n');
}

lval eval_op(lval x, char *op, lval y) {

  /* if either value is an error return it */
  if (x.type == LVAL_ERR) {
    return x;
  }
  if (y.type == LVAL_ERR) {
    return y;
  }

  if (strcmp(op, "+") == 0) {
    return lval_num(x.num + y.num);
  }
  if (strcmp(op, "-") == 0) {
    return lval_num(x.num - y.num);
  }
  if (strcmp(op, "*") == 0) {
    return lval_num(x.num * y.num);
  }
  if (strcmp(op, "/") == 0) {
    /* if second operand is zero return an error */
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
  }
  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t *t) {
  /* if tagged as number, return it directly */
  if (strstr(t->tag, "number")) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  /* the operator is always the second child */
  char *op = t->children[1]->contents;

  /* we store the third child in `x` */
  lval x = eval(t->children[2]);

  /* iterate the remaining children and combining */
  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

int main(int argc, char **argv) {

  /* Create some parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  /* Define them with the following language */
  mpca_lang(MPCA_LANG_DEFAULT, "                               \
            number   : /-?[0-9]+/ ;                            \
            operator : '+' | '-' | '*' | '/' ;                 \
            expr     : <number> | '(' <operator> <expr>+ ')' ; \
            lispy    : /^/ <operator> <expr>+ /$/ ;            \
            ",
            Number, Operator, Expr, Lispy);

  /* Print Version and Exit Information */
  puts("Lisp Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit\n");

  /* In a never ending loop */
  while (1) {
    /* Output our prompt and get input*/
    char *input = readline("lispy> ");

    /* Add input to history */
    add_history(input);

    /* Attempt to parse the user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval result = eval(r.output);
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      /* Otherwise print the error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    /* Free retrieved input */
    free(input);
  }

  mpc_cleanup(4, Number, Operator, Expr, Lispy);
  return 0;
}
