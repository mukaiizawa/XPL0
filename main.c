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

// length of identifier table
#define TXMAX 100

// max.no. of digits in numbers
#define NMAX 14

// length of identifiers
#define IMAX 10

// maximum address
#define AMAX 2047

// maximum depth of block nesting
#define LEVMAX 3

// size of code array
#define CMAX 200

enum symbol {
  becomes, beginsym, callsym, comma, constsym, dosym, endsym, eql, geq, gtr,
  ident, ifsym, leq, lparen, lss, minus, neq, nil, number, oddsym, period, plus,
  procsym, rparen, semicolon, slash, thensym, times, varsym, whilesym
};

char *symbol_name[] = {
  ":=", "begin", "call", ",", "const", "do", "end",
  "=", "[", ">", "ident", "if", "[", "(", "<", "-",
  "#", "nil", "number", "odd", "period", "+", "procedure", ")",
  ";", "/", "then", "*", "var", "while"
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

char *mnemonic[] = {
  "LIT", "OPR", "LOD", "STO", "CAL", "INT", "JMP", "JPC"
};

struct instruction {
  enum fct f;
  int l;
  int a;
};

struct tnode {
  char *name;
  enum object kind;
  int val;
  int level;
  int adr;
};

static FILE *in;
static FILE *out;

static char ch = 0x20;
static int tx = 0;    // table contents
static int dx = 0;
static int cx = 0;    // code allocation index
static enum symbol sym;
static char *id;
static int num;
static char a[IMAX];
static struct instruction code[CMAX];
static char *word[NORW];
static enum symbol wsym[NORW];
static struct tnode table[TXMAX];

static void error(int n)
{
  switch (n) {
    case 98: fprintf(stderr, "illegal state\n");
    case 99: fprintf(stderr, "program too long\n");
    default: fprintf(stderr, "error: %d\n", n);
  }
  exit(1);
}

static void gen(enum fct f, int l, int a)
{
  if (cx > CMAX) error(99);
  code[cx].f = f;
  code[cx].l = l;
  code[cx].a = a;
  cx++;
}

void nextch(void)
{
  assert(!feof(in));
  ch = fgetc(in);
  fprintf(out, "%c", ch);
}

void dump_table(void)
{
  fprintf(out, "table {\n");
  for (int i = 0; i < tx; i++)
    fprintf(out, "\t[\
name: %s\t\
obect: %d\t\
level: %d\t\
address: %d\t\
value: %d\
]\n\
", table[i].name, table[i].kind, table[i].level, table[i].adr, table[i].val);
  fprintf(out, "}\n");
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

/*
 * enter symbol table
 */
static void enter(enum object o, int lev)
{
  table[tx].name = id;
  table[tx].kind = o;
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
  tx++;
  dump_table();
}

/*
 * find identifier id in table
 */
static int position(void)
{
  for (int i = 0; i < tx; i++)
    if (strcmp(table[i].name, id) == 0) return i;
  error(1);
  return -1;    // never reached.
}

void constdeclaration(int lev)
{
  if (sym != ident) error(4);
  getsym();
  if (sym != becomes) error(1);
  if (sym != eql) error(3);
  getsym();
  if (sym != number) error(2);
  enter(constant, lev);
  getsym();
}

void vardeclaration(int lev)
{
  if (sym != ident) error(4);
  enter(variable, lev);
  getsym();
}

void listcode(int cx0)
{
  for (int i = cx0; i < cx - 1; i++)
    printf("%d%s%d%d\n", i, mnemonic[code[i].f], 1, code[i].a);
}

static void expression(int lev);

void factor(int lev)
{
  while (true) {
    int pos;
    switch (sym) {
      case ident:
        pos = position();
        if (pos == 0) error(11);
        switch (table[pos].kind) {
          case constant: gen(LIT, 0, table[pos].val);
          case variable: gen(LOD, lev - table[pos].level, table[pos].adr);
          case proc: error(21);
        }
        getsym();
        continue;
      case number:
        if (num > AMAX) error(30);
        gen(LIT, 0, num);
        getsym();
        continue;
      case lparen:
        getsym();
        expression(lev);
        if (sym != rparen) error(22);
        getsym();
        continue;
      default: break;
    }
    break;
  }
}

void term(int lev)
{
  // symbol mulopp;
}

static void expression(int lev)
{
  // symbol addop;
}

void statement(int lev)
{
  // int cx1, cx2;
}

void block(int lev)
{
  // int dx, tx0, cx0;
}

int main (int argc, char **argv)
{
  in = stdin;
  out = stdout;
  word[0] = symbol_name[beginsym];
  word[1] = symbol_name[callsym];
  word[2] = symbol_name[constsym];
  word[3] = symbol_name[dosym];
  word[4] = symbol_name[endsym];
  word[5] = symbol_name[ifsym];
  word[6] = symbol_name[oddsym];
  word[7] = symbol_name[procsym];
  word[8] = symbol_name[thensym];
  word[9] = symbol_name[varsym];
  word[10] = symbol_name[whilesym];
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
  id = "asd";
  enter(proc, 1);
  enter(proc, 1);
  printf("pos: %d\n", position());
  return 0;
}
