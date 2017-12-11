// XPL0 main routine.

/*
 * XPL0 BNF
 * <program> ::= <block> '.'
 * <block> ::= [ 'const' ident '=' number {',' ident '=' number} ';' ]
 *             [ 'var' ident {',' ident} ';' ]
 *             { 'procedure' ident ';' <block> ';' }
 *             <statement>
 * <statement> ::= [ ident ':=' <expression>
 *                   | 'call' ident
 *                   | 'begin' <statement> {';' <statement> } 'end'
 *                   | 'if' <condition> 'then' <statement>
 *                   | 'while' <condition> 'do' <statement> ]
 * <condition> ::= 'odd' <expression>
 *                   | <expression> ('='|'#'|'<'|'<='|'>'|'>=') <expression>
 * <Expression> ::= [ '+' | '-' ] <term> { ( '+' | '-' ) <term> }
 * <term> ::= <factor> { ( '*' | '/' ) <factor> }
 * <factor> ::= ident | number | '(' <expression> ')'
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// no.of reserved words
#define NORW 11

// max.no. of digits in numbers
#define NMAX 14

// length of identifier table
#define TXMAX 100

// length of identifiers
#define IMAX 10

// maximum address
#define AMAX 2047

// maximum depth of block nesting
#define LEVMAX 3

enum symbol {
  becomes, beginsym, callsym, comma, constsym, dosym, endsym, eql, geq, gtr,
  ident, ifsym, leq, lparen, lss, minus, neq, nil, number, oddsym, period, plus,
  procsym, rparen, semicolon, slash, thensym, times, varsym, whilesym
};

enum object {
  constant,
  variable,
  proc
};

enum fct {
  LIT,    // LIT 0, a : load constant a
  OPR,    // OPR 0, a : execute operation a
  LOD,    // LOD l, a : load variable l, a
  STO,    // STO l, a : store variable l, a
  CAL,    // CAL l, a : call procedure a at level l
  INT,    // INT 0, a : increment t-regeister by a
  JMP,    // JMP 0, a : jump to a
  JPC     // JPC 0, a : jump conditional to a
};

struct instruction {
  enum fct f;
  int l;
  int a;
};

struct tnode {
  char *name;
  enum object obj;
  int val;
  int level;
  int adr;
};

static FILE *in;
static FILE *out;

static char ch = 0x20;
static int tx = 0;
static int dx = 0;
static enum symbol sym;
static char *id;
static int num;
static char a[IMAX];
static char *word[NORW];
static enum symbol wsym[NORW];
static struct tnode table[TXMAX];
// static enum symbol declbegsys[] = {constsym, varsym, procsym};
// static enum symbol statbegsys[] = {beginsym, callsym, ifsym, whilesym};
// static enum symbol facbegsys[] = {ident, number, lparen};

int symsetlen(enum symbol *symset)
{
  return sizeof(symset) / sizeof(symset[0]);
}

void error(int n)
{
  fprintf(stderr,"error: %d\n", n);
  exit(1);
}

void test(enum symbol *symset)
{
  for (int i = 0; i < symsetlen(symset); i++)
    if (sym == symset[i]) return;
  fprintf(stderr, "symbol %d must be[", sym);
  for (int i = 0; i < symsetlen(symset); i++)
    fprintf(stderr, " %d", symset[i]);
  fprintf(stderr, " ]\n");
  error(0);
}

void nextch(void)
{
  assert(!feof(in));
  ch = fgetc(in);
  fprintf(out, "%c", ch);
}

void dump_table(void)
{
  printf("table {");
  for (int i = 0; i < tx; i++) {
    printf("\tlevel: %d\n", table[i].level);
    printf("\tadr: %d\n", table[i].adr);
    printf("\tval: %d\n", table[i].val);
  }
  printf("}");
}

void getsym(void)
{
  while (isspace(ch)) nextch();
  if (isalpha(ch)) {
    sym = ident;
    for (int i = 0; isalnum(ch); i++) {
      assert(i < IMAX);
      a[i] = ch;
      nextch();
    }
    id = a;
    for (int i = 0; i < NORW; i++)
      if (strcmp(id, word[i]) == 0) {
        sym = wsym[i];
        break;
      }
  } else if (isdigit(ch)) {
    num = 0;
    sym = number;
    while (isdigit(ch)) {
      num = 10 * num + (ch - '0');
      nextch();
    }
  } else {
    switch (ch) {
      case '+': sym = plus; break;
      case '-': sym = minus; break;
      case '*': sym = times; break;
      case '/': sym = slash; break;
      case '(': sym = lparen; break;
      case ')': sym = rparen; break;
      case '=': sym = eql; break;
      case ',': sym = comma; break;
      case '.': sym = period; break;
      case '#': sym = neq; break;
      case '<': sym = lss; break;
      case '>': sym = gtr; break;
      case '[': sym = leq; break;
      case ']': sym = geq; break;
      case ';': sym = semicolon; break;
      case ':':
        nextch();
        if (ch == '=') {
          sym = becomes;
          break;
        }
      default: sym = nil;
    }
  }
}

static void enter(enum object o, int lev)
{
  tx++;
  table[tx].name = id;
  table[tx].obj = o;
  switch (o) {
    case constant:
      assert(num < AMAX);
      table[tx].val = num;
      break;
    case variable:
      table[tx].level = lev;
      table[tx].adr = dx++;
      break;
    case proc:
      table[tx].level = lev;
      break;
  }
}

/*
 * find identifier id in table
 */
static int position()
{
  for (int i = 0; i < tx; i++)
    if (strcmp(table[i].name, id) == 0) return i;
  error(1);
  return -1;    // never reached.
}

void block(int lev, enum symbol *symset)
{
  // int dx, tx0, cx0;
}

int main (int argc, char **argv)
{
  in = stdin;
  out = stdout;
  word[0] = "begin";
  word[1] = "call";
  word[2] = "const";
  word[3] = "do";
  word[4] = "end";
  word[5] = "if";
  word[6] = "odd";
  word[7] = "procedure";
  word[8] = "then";
  word[9] = "var";
  word[10] = "while";
  wsym[0] = beginsym;
  wsym[1] = callsym;
  wsym[2] = constsym;
  wsym[3] = dosym;
  wsym[4] = endsym;
  wsym[5] = ifsym;
  wsym[6] = oddsym;
  wsym[7] = procsym;
  wsym[8] = thensym;
  wsym[9] = varsym;
  wsym[10] = whilesym;
  getsym();
  fprintf(out, "%d\n", sym);
  test((enum symbol[]){ident, oddsym});
  id = "asd";
  enter(variable, 2);
  dump_table();
  printf("%d\n", position());
  return 0;
}
