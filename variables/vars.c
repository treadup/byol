#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include <editline/history.h>

#include <errno.h>

#include "mpc.h"

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Create enumeration of possible lval types. */
enum { LVAL_ERR=0, LVAL_NUM=1, LVAL_SYM=2,
       LVAL_FUN=3, LVAL_SEXPR=4, LVAL_QEXPR=5 };

/* Create enumeration of possible error types. */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

typedef lval*(*lbuiltin)(lenv*, lval*);

/* Declare new lval struct. */
struct lval {
    int type;

    long num;
    char* err;
    char* sym;
    lbuiltin fun;

    int count;
    lval** cell;
};

struct lenv {
    int count;
    char** syms;
    lval** vals;
}

lval* lval_eval(lval* v);
lval* builtin_op(lval* a, char *op);
lval* builtin(lval* a, char* func);

/* Environment functions */

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e) {
    for(int i = 0; i < e-count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lval_get(lenv* e, lval* k) {

    /* Iterate over all items in environment */
    for(int i = 0; i < e->count; i++) {

        /* Check if the stored string matches the symbol string */
        /* If it does return a copy of the value */
        if(strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }

    /* If no symbol found return an error */
    return lval_err("Unbound symbol");
}

void lenv_put(lenv* e, lval* k, lval* v) {

    /* Iterate over all items in the environment */
    /* Check if the variable already exists */
    for (int i = 0; i < e->count; i++) {

        /* If variable is found delete item at that position */
        /* and replace with variable supplied by the user */
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    /* If no existing entry found allocate space for new entry */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    /* Copy contents of lval and symbol string into new location */
    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

/* Construct a pointer to a new Number lval */
lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Construct a pointer to a new Error lval */
lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

/* Construct a pointer to a new Symbol lval. */
lval* lval_sym(char *s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

/* Construct a pointer to a new Function lval. */
lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
    return v;
}

/* Construct a pointer to a new empty Sexpr lval */
lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Construct a pointer to a new empty Qexpr lval */
lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval* v) {
    switch(v->type) {
        /* Do nothing special for Number type */
        case LVAL_NUM:
            break;

        /* For Err free the string data */
        case LVAL_ERR:
            free(v->err);
            break;

        /* For Sym free the string data */
        case LVAL_SYM:
            free(v->sym);
            break;

        /* Do nothing special for Function type */
        case LVAL_DEL:
            break;

        /* For Sexpr delete all elements inside */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for(int i = 0; i < v->count; i++ ) {
                lval_del(v->cell[i]);
            }

            /* Also free the memory allocated to contain the pointers */
            free(v->cell);

            break;
    }

    /* Free the memory allocated for the lval struct itself. */
    free(v);
}

lval* lval_pop(lval* v, int i) {

    /* Finds the item at "i" */
    lval* x = v->cell[i];

    /* Shift memory after the item "i" over the top */
    memmove(&v->cell[i], &v->cell[i+1],
            sizeof(lval*) * (v->count - i - 1));

    /* Decrease the count of items in the list */
    v->count--;

    /* Reallocate the memory used */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval* lval_copy(lval* v) {

    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch(v->type) {
        /* Copy functions and numbers directly */
        case LVAL_FUN:
            x->fun = v->fun;
            break;
        case LVAL_NUM:
            x->num = v->num;
            break;

        /* Copy strings using malloc and strcpy */
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;

        case LVAL_SYM:;
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;

        /* Copy lists by copying each subexpression */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for(int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

    return x;
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);

    if(errno != ERANGE) {
        return  lval_num(x);
    } else {
        return lval_err("invalid number");
    }
}

lval* lval_read(mpc_ast_t* t) {
    /* If symbol or number return conversion of that type. */
    if(strstr(t->tag, "number")) {
        return lval_read_num(t);
    } else if(strstr(t->tag, "symbol")) {
        return lval_sym(t->contents);
    }

    /* If root or sexpr create empty list */
    lval* x = NULL;
    if(strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if(strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
    if(strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {

        if(strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if(strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if(strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if(strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if(strcmp(t->children[i]->tag, "regex") == 0) { continue; }

        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

/* Put this declaration in a header file . */
void lval_expr_print(lval* v, char open, char close);

/* Print an lval */
void lval_print(lval* v) {
    switch(v->type) {
        case LVAL_NUM:
            printf("%li", v->num);
            break;
        case LVAL_ERR:
            printf("Error: %s", v->err);
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_FUN:
            printf("<function>");
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
    }
}

/* Print an Lval expression */
void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {

        /* Print Value contained within */
        lval_print(v->cell[i]);

        /* Do not print trailing space if last element. */
        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_debug_print(lval* v) {
   /* I could either implement this or I could use GDB */
   /* This might be a good oppurtunity to learn GDB and valgrind. */
}

/* Print an lval followed by a newline */
void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

lval* lval_eval_sexpr(lenv* e, lval* v) {

    /* Evaluate children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    /* Error Checking */
    for(int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    /* Empty Expression */
    if(v->count == 0) {
        return v;
    }

    /* Single Expression */
    if(v->count == 1) {
        return lval_take(v, 0);
    }

    /* Ensure first element is a function after evaluation */
    lval* f = lval_pop(v, 0);
    if(f->type != LVAL_FUN) {
        lval_del(v);
        lval_del(f);
        return lval_err("First element is not a function");
    }

    /* Call builtin with operator */
    lval* result = f->fun(e, v);
    lval_del(f);
    return result;
}

lval* lval_eval(lenv* e, lval* v) {
    if(v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    if(v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(v);
    } else {
        return v;
    }
}

#define LASSERT(args, cond, err) if(!(cond)) { lval_del(args); return lval_err(err); }

lval* builtin_op(lval* a, char *op) {

    /* Ensure all arguments are numbers */
    for(int i = 0; i < a->count; i++) {
        if(a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non number.");
        }
    }

    /* Pop the first element */
    lval* x = lval_pop(a, 0);

    /* If no arguments and sub then perform unary negation */
    if((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    /* While there are still elements remaining */
    while(a->count > 0) {

        /* Pop the next element */
        lval* y = lval_pop(a, 0);

        if(strcmp(op, "+") == 0) { x->num += y->num; }
        if(strcmp(op, "-") == 0) { x->num -= y->num; }
        if(strcmp(op, "*") == 0) { x->num *= y->num; }
        if(strcmp(op, "/") == 0) {
            if(y->num == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division by Zero"); break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }

    lval_del(a);
    return x;
}

lval* builtin_head(lval* a) {
    LASSERT(a, a->count == 1, "Function 'head' passed too many arguments.");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'head' passed incorrect types");
    LASSERT(a, a->cell[0]->count > 0,
        "Function 'head' passed {}");

    /* Otherwise take first argument */
    lval* v = lval_take(a, 0);

    /* Delete all elements that are not head and return */
    while(v->count > 1) {
        lval_del(lval_pop(v, 1));
    }

    return v;
}

lval* builtin_tail(lval* a) {
    LASSERT(a, a->count == 1,
        "Function 'tail' passed too many arguments.");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Function 'tail' was passed incorrect types.");
    LASSERT(a, a->cell[0]->count > 0,
        "Function 'tail' passed {}");

    /* Take the first argument */
    lval* v = lval_take(a, 0);

    /* Delete first element and return */
    lval_del(lval_pop(v, 0));

    return v;
}

lval* builtin_list(lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lval* a) {
    LASSERT(a, a->count == 1,
            "Function 'eval' passed too many arguments");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' passed incorrect type");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval* lval_join(lval* x, lval* y) {

    /* For each cell in 'y' add it to 'x' */
    while(y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    /* Delete the empty y and return x */
    lval_del(y);

    return x;
}

lval* builtin_join(lval* a) {

    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
                "Function 'join' passed incorrect type.");
    }

    lval* x = lval_pop(a, 0);

    while(a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);

    return x;
}

lval* builtin(lval* a, char* func) {
    if(strcmp("list", func) == 0) { return builtin_list(a); }
    if(strcmp("head", func) == 0) { return builtin_head(a); }
    if(strcmp("tail", func) == 0) { return builtin_tail(a); }
    if(strcmp("join", func) == 0) { return builtin_join(a); }
    if(strcmp("eval", func) == 0) { return builtin_eval(a); }
    if(strstr("+-/*", func)) { return builtin_op(a, func); }
    lval_del(a);
    return lval_err("Unknown function");
}

int main(int argc, char** argv) {
    printf("Welcome to Lispy 0.1\n");
    /* Create some parsers */
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Qexpr  = mpc_new("qexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Lispy  = mpc_new("lispy");

    /* Define them with the following grammar. */
    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                    \
          number : /-?[0-9]+/ ;                              \
          symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/          \
          sexpr  : '(' <expr>* ')' ;                         \
          qexpr  : '{' <expr>* '}' ;                         \
          expr   : <number> | <symbol> | <sexpr> | <qexpr> ; \
          lispy  : /^/ <expr>* /$/ ;                         \
        ",
	Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    /* Print version and Exit information */
    puts("Lispy Version 0.0.0.0.1");
    puts("Press Ctrl+C to Exit\n");

    while(1) {

        /* Output the prompt and get the input. */
        char *input = readline("lispy> ");

        if(strcmp(input, "\\quit") == 0) {
            break;
        }

        /* Add input to history */
        add_history(input);

        /* Attempt to parse the user input */
        mpc_result_t r;
        if(mpc_parse("<stdin>", input, Lispy, &r)) {
            /* On Success Evaluate the AST */
            lval* x = lval_read(r.output);
            x = lval_eval(x);
	    lval_println(x);
            lval_del(x);
        } else {
            /* Otherwise Print the Error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        /* Free the retrieved input. */
        free(input);
    }

    /* Undefine and delete our parsers. */
    mpc_cleanup(5, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}
