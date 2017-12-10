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

// no.of reserved words
#define NORW 11

// max.no. of digits in numbers
#define NMAX 14

// length of identifiers
#define IMAX 10

// maximum address
#define AMAX 2047

// maximum depth of block nesting
#define LEVMAX 3

enum symbol {
  nil,
  ident,
  number,
  plus,
  minus,
  times,
  slash,
  oddsym,
  eql,
  neq,
  lss,
  leq,
  gtr,
  geq,
  lparen,
  rparen,
  comma,
  semicolon,
  period,
  becomes,
  beginsym,
  endsym,
  ifsym,
  thensym,
  whilesym,
  dosym,
  callsym,
  constsym,
  varsym,
  procsym
};

enum object {
  constant,
  variable,
  procedure
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

static FILE *in;
static FILE *out;

static char ch;
static enum symbol sym;
static char *id;
static int num;
static char a[IMAX];
static char *word[NORW];
static enum symbol wsym[NORW];

void error(int n)
{
  fprintf(stderr,"error: %d\n", n);
  exit(1);
}

void nextch(void)
{
  assert(!feof(in));
  ch = fgetc(in);
  fprintf(out, "%c", ch);
}

void getsym(void)
{
  int i;
  while (isspace(ch)) nextch();
  if (isalpha(ch)) {
    sym = ident;
    i = 0;
    while (isalnum(ch)) {
      assert(i < IMAX);
      a[++i] = ch;
      nextch();
    }
    id = a;
    for (i = 0; i < NORW; i++)
      if (strcmp(id, word[i]) == 0) {
        sym = wsym[i];
        break;
      }
  } else if (isdigit(ch)) {
    i = num = 0;
    sym = number;
    while (isdigit(ch)) {
      num = 10 * num + (ch - '0');
      i++;
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
      case ':': nextch();
                if (ch == '=') {
                  sym = becomes;
                  break;
                }
      default: sym = nil;
    }
  }
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
  ch = '\n';
  getsym();
  fprintf(out, "%d\n", sym);
  return 0;
}
