# include <stdio.h>
# include <stdlib.h>
# include <stdarg.h>
# include <string.h>
# include <math.h>
# include "abstract_syntax_tree.h"

/* simple symtab of fixed size */
#define NHASH 9997
struct symbol symtab[NHASH];

/* symbol table */
/* hash a symbol */
static unsigned symhash(char *sym)
{
 unsigned int hash = 0;
 unsigned c;
 while(c = *sym++) hash = hash*9 ^ c;
 return hash;
}

/* Build an AST */
struct ast *newast(int nodetype, struct ast *l, struct ast *r) {
    struct ast *a = (struct ast *)malloc(sizeof(struct ast));
    if (!a) {
        yyerror("Out of memory");
        exit(1);
    }
    a->nodetype = nodetype;
    a->l = l;
    a->r = r;
    return a;
}

struct symbol *lookup(char* sym)
{
 struct symbol *sp = &symtab[symhash(sym)%NHASH];
 int scount = NHASH; /* how many have we looked at */
 while(--scount >= 0) {
    if(sp->name && !strcmp(sp->name, sym)) { return sp; }
 if(!sp->name) { /* new entry */
 sp->name = strdup(sym);
 sp->value = 0;
 sp->type = 0;
 sp->func = NULL;
 sp->syms = NULL;
 return sp;
 }
 if(++sp >= symtab+NHASH) sp = symtab; /* try the next entry */
 }
 yyerror("symbol table overflow\n");
 abort(); /* tried them all, table is full */
}

struct ast *newnum(double d) {
    struct numval *a = (struct numval *)malloc(sizeof(struct numval));
    if (!a) {
        yyerror("Out of memory");
        exit(1);
    }
    a->nodetype = 'K';
    a->number = d;
    return (struct ast *)a;
}

struct ast *newcmp(int cmptype, struct ast *l, struct ast *r)
{
    struct ast *a = (struct ast *)malloc(sizeof(struct ast));
    if(!a) {
    yyerror("out of space");
    exit(0);
    }
    a->nodetype = '0' + cmptype;
    a->l = l;
    a->r = r;
    return a;
}

struct ast *newdeclare(struct symbol *name, struct symlist *args, struct ast *body) {
    struct ufncall *decl = (struct ufncall *)malloc(sizeof(struct ufncall));
    if (!decl) {
        yyerror("out of space");
        exit(0);
    }

    decl->nodetype = 'D'; // 'D' per "declaration"
    decl->s = name;       // Simbolo che rappresenta il nome della funzione
    decl->l = body;       // Corpo della funzione (AST dei suoi statements)

    // Assegna gli argomenti alla funzione
    if (name->syms) {
        symlistfree(name->syms); // Libera eventuali argomenti precedenti
    }
    name->syms = args;

    return (struct ast *)decl;
}


struct ast *newfunc(int functype, struct ast *l)
{
 struct fncall *a = malloc(sizeof(struct fncall));

 if(!a) {
 yyerror("out of space");
 exit(0);
 }
 a->nodetype = 'F';
 a->l = l;
 a->functype = functype;
 return (struct ast *)a;
}

struct ast *newcall(struct symbol *s, struct ast *l)
{
 struct ufncall *a = malloc(sizeof(struct ufncall));

 if(!a) {
 yyerror("out of space");
 exit(0);
 }
 a->nodetype = 'C';
 a->l = l;
 a->s = s;
 return (struct ast *)a;
}

struct ast *newref(struct symbol *s)
{
 struct symref *a = malloc(sizeof(struct symref));

 if(!a) {
 yyerror("out of space");
 exit(0);
 }
 a->nodetype = 'N';
 a->s = s;
 return (struct ast *)a;
}

struct ast *newasgn(struct symbol *s, struct ast *v)
{
 struct symasgn *a = malloc(sizeof(struct symasgn));
if(!a) {
 yyerror("out of space");
 exit(0);
 }
 a->nodetype = '=';
 a->s = s;
 a->v = v;
 return (struct ast *)a;
}

struct ast *newflow(int nodetype, struct ast *cond, struct ast *tl, struct ast *el)
{
 struct flow *a = malloc(sizeof(struct flow));

 if(!a) {
 yyerror("out of space");
 exit(0);
 }
 a->nodetype = nodetype;
 a->cond = cond;
 a->tl = tl;
 a->el = el;
 return (struct ast *)a;
}

struct ast *newstr(char *s) {
    struct ast *a = malloc(sizeof(struct ast));
    if (!a) {
        yyerror("out of space");
        exit(0);
    }
    a->nodetype = 'S'; 
    a->s = strdup(s); // Copia la stringa
    free(s); // Libera la stringa originale
    return (struct ast *)a;
}

struct symlist *newsymlist(struct symbol *sym, struct symlist *next)
{
 struct symlist *sl = malloc(sizeof(struct symlist));

 if(!sl) {
 yyerror("out of space");
 exit(0);
 }
 sl->sym = sym;
 sl->next = next;
 return sl;
}

/* free a list of symbols */
void symlistfree(struct symlist *sl)
{
 struct symlist *nsl;
 while(sl) {
 nsl = sl->next;
 free(sl);
 sl = nsl;
 }
}

/* free a tree of ASTs */
void
treefree(struct ast *a)
{
 switch(a->nodetype) {
 /* two subtrees */
 case '+':
 case '-':
 case '*':
 case '/':
 case '^':
 case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': 
 case 'L':
 treefree(a->r);
 /* one subtree */
 case '|':
 case 'M': case 'C': case 'F':
 treefree(a->l);
 /* no subtree */
 case 'K': case 'N': 
 break;
 case 'S': 
 if (a->s) free(a->s); break;
 case '=':
 free( ((struct symasgn *)a)->v);
 break;
 /* up to three subtrees */
  case 'I': case 'W':
 free( ((struct flow *)a)->cond);
 if( ((struct flow *)a)->tl) treefree( ((struct flow *)a)->tl);
 if( ((struct flow *)a)->el) treefree( ((struct flow *)a)->el);
 break;
 default: printf("internal error: free bad node %c\n", a->nodetype);
 }

 free(a); /* always free the node itself */
}

static double callbuiltin(struct fncall *);
static double calluser(struct ufncall *);

double eval(struct ast *a)
{
 double v;
 if(!a) {
 yyerror("internal error, null eval");
 return 0.0;
 }
 switch(a->nodetype) {
 /* constant */
 case 'K': v = ((struct numval *)a)->number; break;
 /* string */
 case 'S': {
            printf("String: %s\n", a->s);
            return 0;
        }
 /* name reference */
 case 'N': v = ((struct symref *)a)->s->value; break;
 /* assignment */
 case '=': 

 struct symbol *sym = ((struct symasgn *)a)->s;
 struct ast *val = ((struct symasgn *)a)->v;

 if ((sym->type == 1 && val->nodetype != 'K') || // num
            (sym->type == 2 && val->nodetype != 'S')) { // str
            yyerror("Type mismatch for variable '%s'", sym->name);
            return 0.0;
        }

        // Assegna il valore
        v = sym->value = eval(val); 
 
 break;
 /* expressions */
 case '+': v = eval(a->l) + eval(a->r); break;
 case '-': v = eval(a->l) - eval(a->r); break;
 case '*': v = eval(a->l) * eval(a->r); break;
 case '/': v = eval(a->l) / eval(a->r); break;
 case '|': v = fabs(eval(a->l)); break;
 case '^': v = pow(eval(a->l), eval(a->r)); break; 
 case 'M': v = -eval(a->l); break;
 /* comparisons */
 case '1': v = (eval(a->l) > eval(a->r))? 1 : 0; break;
 case '2': v = (eval(a->l) < eval(a->r))? 1 : 0; break;
 case '3': v = (eval(a->l) != eval(a->r))? 1 : 0; break;
 case '4': v = (eval(a->l) == eval(a->r))? 1 : 0; break;
 case '5': v = (eval(a->l) >= eval(a->r))? 1 : 0; break;
 case '6': v = (eval(a->l) <= eval(a->r))? 1 : 0; break;
 /* control flow */
 /* null expressions allowed in the grammar, so check for them */
 /* if/then/else */
 case 'I':
 if( eval( ((struct flow *)a)->cond) != 0) { 
 if( ((struct flow *)a)->tl) { 
 v = eval( ((struct flow *)a)->tl);
 } else
 v = 0.0; /* a default value */
 } else {
 if( ((struct flow *)a)->el) { 
 v = eval(((struct flow *)a)->el);
 } else
 v = 0.0; /* a default value */
 }
 break;
 /* while/do */
  case 'W':
 v = 0.0; /* a default value */

 if( ((struct flow *)a)->tl) {
 while( eval(((struct flow *)a)->cond) != 0) 
 v = eval(((struct flow *)a)->tl); 
 }
 break; /* value of last statement is value of while/do */

 /* list of statements */
 case 'L': eval(a->l); v = eval(a->r); break;
 case 'F': v = callbuiltin((struct fncall *)a); break;
 case 'C': v = calluser((struct ufncall *)a); break;
 default: printf("internal error: bad node %c\n", a->nodetype);
 }
 return v;
}

static double
callbuiltin(struct fncall *f)
{
 enum bifs functype = f->functype;
 double v = eval(f->l);
 switch(functype) {
 case B_sqrt:
 return sqrt(v);
 case B_exp:
 return exp(v);
 case B_log:
 return log(v);
 case B_print:
 printf("= %4.4g\n", v);
 return v;
 default:
 yyerror("Unknown built-in function %d", functype);
 return 0.0;
 }
}

/* define a function */
void
dodef(struct symbol *name, struct symlist *syms, struct ast *func)
{
 if(name->syms) symlistfree(name->syms);
 if(name->func) treefree(name->func);
 name->syms = syms;
 name->func = func;
}

static double
calluser(struct ufncall *f)
{
 struct symbol *fn = f->s; /* function name */
 struct symlist *sl; /* dummy arguments */
 struct ast *args = f->l; /* actual arguments */
 double *oldval, *newval; /* saved arg values */
 double v;
 int nargs;
 int i;
 if(!fn->func) {
 yyerror("call to undefined function", fn->name);
 return 0;
 }
 /* count the arguments */
 sl = fn->syms;
 for(nargs = 0; sl; sl = sl->next)
 nargs++;
 /* prepare to save them */
 oldval = (double *)malloc(nargs * sizeof(double));
 newval = (double *)malloc(nargs * sizeof(double));
 if(!oldval || !newval) {
 yyerror("Out of space in %s", fn->name); return 0.0;
 }

 /* evaluate the arguments */
 for(i = 0; i < nargs; i++) {
 if(!args) {
 yyerror("too few args in call to %s", fn->name);
 free(oldval); free(newval);
 return 0.0;
 }
 if(args->nodetype == 'L') { /* if this is a list node */
 newval[i] = eval(args->l);
 args = args->r;
 } else { /* if it's the end of the list */
 newval[i] = eval(args);
 args = NULL;
 }
 }

 /* save old values of dummies, assign new ones */
 sl = fn->syms;
 for(i = 0; i < nargs; i++) {
 struct symbol *s = sl->sym;
 oldval[i] = s->value;
 s->value = newval[i];
 sl = sl->next;
 }
 free(newval);
 /* evaluate the function */
 v = eval(fn->func);
 /* put the real values of the dummies back */
 sl = fn->syms;
 for(i = 0; i < nargs; i++) {
 struct symbol *s = sl->sym;
 s->value = oldval[i];
 sl = sl->next;
 }
 free(oldval);
 return v;
}

/* Error reporting function */
void yyerror(const char *s, ...) {
    va_list ap;
    va_start(ap, s);
    fprintf(stderr, "%d: Error: ", yylineno);
    vfprintf(stderr, s, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

// Funzione ricorsiva per stampare l'AST
void print_ast(struct ast *node, int depth, char *prefix) {
    if (node == NULL) {
        return;
    }

    // Stampa il ramo con il prefisso
    printf("%s", prefix);

    // Aggiungi il nodo corrente
    if (depth == 0) {
        printf("Root -> ");
    } else {
        printf("|__ ");
    }

    // Stampa il contenuto del nodo
    if (node->s != NULL) {
        printf("Node type: %c, Value: %s\n", node->nodetype, node->s);
    } else {
        printf("Node type: %c\n", node->nodetype);
    }

    // Costruisci il nuovo prefisso per i figli
    char new_prefix[256];
    snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, (depth == 0) ? "" : "    ");

    // Stampa i figli
    if (node->l && node->r) {
        print_ast(node->l, depth + 1, new_prefix);
        print_ast(node->r, depth + 1, new_prefix);
    }
}