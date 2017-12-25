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
 * <expression> ::= [ '+' | '-' ] <term> { ( '+' | '-' ) <term> }
 * <term> ::= <factor> { ( '*' | '/' ) <factor> }
 * <factor> ::= ident | number | '(' <expression> ')'
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TABLE_SIZE 100
#define MAX_IDENTIFIER 10
#define MAX_ADDRESS 2048
#define CMAX 200

enum symbol {
  becomes, beginsym, callsym, comma, constsym, dosym, endsym, eql, geq, gtr,
  ident, ifsym, leq, lparen, lss, minus, neq, number, oddsym, period, plus,
  procsym, rparen, semicolon, slash, thensym, times, varsym, whilesym, nil
};
char *symbol_name[] = {
  ":=", "begin", "call", ",", "const", "do", "end", "=", "[", ">", "ident",
  "if", "[", "(", "<", "-", "#", "number", "odd", "period", "+",
  "procedure", ")", ";", "/", "then", "*", "var", "while", "nil"
};

enum object { constant, variable, proc };
char *object_name[] = { "constant", "variable", "proc" };

enum fct {
  LIT,    // [LIT, 0, a] load constant a
  OPR,    // [OPR, 0, a] execute operation a
  LOD,    // [LOD, l, a] load variable l, a
  STO,    // [STO, l, a] store variable l, a
  CAL,    // [CAL, l, a] call procedure a at level l
  INT,    // [INT, 0, a] increment t-regeister by a
  JMP,    // [JMP, 0, a] jump to a
  JPC     // [JPC, 0, a] jump conditional to a
};
char *nemonic[] = { "LIT", "OPR", "LOD", "STO", "CAL", "INT", "JMP", "JPC" };

struct inst {
  enum fct f;
  int l;
  int a;
};
static struct inst code[CMAX];

struct table_record {
  char name[MAX_IDENTIFIER];
  enum object kind;
  int val;
  int level;
  int adr;
};
static struct table_record table[TABLE_SIZE];

static FILE *in;
static FILE *out;
static FILE *err;

static int lex_ch;
static int lex_line;
static int lex_column;
static enum symbol lex_sym;
static int lex_num;
static char lex_str[MAX_IDENTIFIER];

static int tx = 0;    // table index
static int dx = 0;    // data allocation index
static int cx = 0;    // code allocation index

static void listcode(int cx0)
{
  for (int i = cx0; i < cx; i++)
    fprintf(out, "%4d: %s %d,%d\n", i, nemonic[code[i].f], code[i].l
        , code[i].a);
}

static void dump(void)
{
  listcode(0);
  fprintf(out, "table {\n");
  for (int i = 0; i < tx; i++)
    fprintf(out, "\t[\
name: %s\t\
object: %s\t\
level: %d\t\
address: %d\t\
value: %d\
]\n"
    , table[i].name
    , object_name[table[i].kind]
    , table[i].level
    , table[i].adr
    , table[i].val);
  fprintf(out, "}\n");
}

static void error(int n)
{
  char* msg = NULL;
  switch (n) {
    case 1: msg = "Use '=' instead of ':='"; break;
    case 2: msg = "'=' must be followed by a number"; break;
    case 3: msg = "Identifier must be followed by '='"; break;
    case 4: msg = "';' missing"; break;
    case 5: msg = "',' or ';' missing"; break;
    case 6: msg = "Incorrect symbol after procedure declaration"; break;
    case 7: msg = "Statement expecte"; break;
    case 8: msg = "Incorrect symbol after statement part in block"; break;
    case 9: msg = "'.' expected"; break;
    case 10: msg = "';' between statements missing"; break;
    case 11: msg = "Undeclared identifier"; break;
    case 12: msg = "Assignment to constant or procedure is not allowed"; break;
    case 13: msg = "':=' expected"; break;
    case 14: msg = "Call must be followed by an identifie"; break;
    case 15: msg = "Call of a constant or variable is meaningless"; break;
    case 16: msg = "'then' expecte"; break;
    case 17: msg = "';' or 'end' expecte"; break;
    case 18: msg = "'do' expecte"; break;
    case 20: msg = "Relational operator expected"; break;
    case 21: msg = "Expression must not contain a procedure identifier"; break;
    case 22: msg = "')' missing"; break;
    case 23: msg = "An expression cannot begin with this symbol"; break;
    case 24: msg = "This Identifier is too large"; break;
    case 25: msg = "eof reached"; break;
    case 26: msg = "Factor expected"; break;
    case 27: msg = "'const' must be followed by identifier"; break;
    case 28: msg = "'var' must be followed by identifier"; break;
    case 29: msg = "'procedure' must be followed by identifier"; break;
    case 30: msg = "This number is too large"; break;
    default: msg = "error";
  }
  fprintf(err, "\nError: %s.\n", msg);
  fprintf(err, "at line: %d, column: %d\n", lex_line, lex_column);
  fprintf(err, "symbol: '%s', number: '%i', string: '%s'\n"
      , symbol_name[lex_sym], lex_num, lex_str);
  dump();
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
  if (feof(in)) error(25);
  if ((lex_ch = fgetc(in)) != '\n') lex_column++;
  else {
    lex_line++;
    lex_column = 0;
  }
}

enum symbol getsym(void)
{
  while (isspace(lex_ch)) nextch();
  if (isalpha(lex_ch)) {
    lex_sym = ident;
    for (int i = 0; isalnum(lex_ch); i++) {
      if ((i + 1) >= MAX_IDENTIFIER) error(24);
      lex_str[i] = lex_ch;
      lex_str[i + 1] = '\0';
      nextch();
    }
    for (int i = 0; i != nil ; i++)
      if (strcmp(lex_str, symbol_name[i]) == 0) {
        lex_sym = i;
        break;
      }
  } else if (isdigit(lex_ch)) {
    lex_num = 0;
    lex_sym = number;
    while (isdigit(lex_ch)) {
      lex_num = 10 * lex_num + (lex_ch - '0');
      nextch();
    }
  } else {
    switch (lex_ch) {
      case '+': lex_sym = plus; break;
      case '-': lex_sym = minus; break;
      case '*': lex_sym = times; break;
      case '/': lex_sym = slash; break;
      case '(': lex_sym = lparen; break;
      case ')': lex_sym = rparen; break;
      case '=': lex_sym = eql; break;
      case ',': lex_sym = comma; break;
      case '.': lex_sym = period; break;
      case '#': lex_sym = neq; break;
      case '<': lex_sym = lss; break;
      case '>': lex_sym = gtr; break;
      case '[': lex_sym = leq; break;
      case ']': lex_sym = geq; break;
      case ';': lex_sym = semicolon; break;
      case ':':
        nextch();
        if (lex_ch == '=') {
          lex_sym = becomes; break;
        }
      default: lex_sym = nil; break;
    }
    nextch();
  }
  return lex_sym;
}

static void enter(enum object o, int lev)
{
  strcpy(table[tx].name, lex_str);
  table[tx].kind = o;
  switch (o) {
    case constant:
      if(lex_num > MAX_ADDRESS) error(30);
      table[tx].val = lex_num;
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
}

static int position(void)
{
  for (int i = 0; i < tx; i++)
    if (strcmp(table[i].name, lex_str) == 0) return i;
  error(11);
  return -1;    // never reached.
}

static void constdeclaration(int lev)
{
  while (lex_sym != semicolon) {
    if (getsym() != ident) error(27);
    if (getsym() == becomes) error(1);
    else if (lex_sym != eql) error(3);
    if (getsym() != number) error(2);
    enter(constant, lev);
    if (getsym() != comma && lex_sym != semicolon) error(5);
  }
  getsym();
}

static void vardeclaration(int lev)
{
  while (lex_sym != semicolon) {
    if (getsym() != ident) error(28);
    enter(variable, lev);
    if (getsym() != comma && lex_sym != semicolon) error(5);
  }
  getsym();
}

static void expression(int lev);

static void factor(int lev)
{
  switch (lex_sym) {
    case ident:
      {
        int pos = position();
        switch (table[pos].kind) {
          case constant: gen(LIT, 0, table[pos].val); break;
          case variable: gen(LOD, lev - table[pos].level, table[pos].adr); break;
          case proc: error(21); break;
        }
        getsym();
        break;
      }
    case number:
      gen(LIT, 0, lex_num);
      getsym();
      break;
    case lparen:
      getsym();    // (
      expression(lev);
      if (lex_sym != rparen) error(22);
      getsym();    // )
      break;
    default: error(26);
  }
}

static void term(int lev)
{
  enum symbol mulopp;
  factor(lev);
  while (lex_sym == times || lex_sym == slash) {
    mulopp = lex_sym;
    getsym();
    factor(lev);
    if (mulopp == times) gen(OPR, 0, 4);
    else gen(OPR, 0, 5);
  }
}

static void expression(int lev)
{
  enum symbol addop;
  if (lex_sym == plus || lex_sym == minus) {
    addop = lex_sym;
    getsym();
    term(lev);
    if (addop == minus) gen(OPR, 0, 1);
  } else  term(lev);
  while (lex_sym == plus || lex_sym == minus) {
    addop = lex_sym;
    getsym();
    term(lev);
    if (addop == plus) gen(OPR, 0, 2);
    else gen(OPR, 0, 3);
  }
}

static void condition(int lev)
{
  enum symbol relop;
  if (lex_sym == oddsym) {
    getsym();
    expression(lev);
    gen(OPR, 0, 6);
  } else {
    expression(lev);
    relop = lex_sym;
    switch (relop) {
      case eql: case neq: case lss: case leq: case gtr: case geq: break;
      default: error(20);
    }
    getsym();
    expression(lev);
    switch (relop) {
      case eql: gen(OPR, 0, 8); break;
      case neq: gen(OPR, 0, 9); break;
      case lss: gen(OPR, 0, 10); break;
      case geq: gen(OPR, 0, 11); break;
      case gtr: gen(OPR, 0, 12); break;
      case leq: gen(OPR, 0, 13); break;
      default: break;    // unreachable
    }
  }
}

static void statement(int lev)
{
  int cx1, cx2, pos;
  switch (lex_sym) {
    case ident:
      pos = position();
      if (table[pos].kind != variable) error(12);
      if (getsym() != becomes) error(13);
      getsym();
      expression(lev);
      gen(STO, lev - table[pos].level, table[pos].adr);
      break;
    case callsym:
      if (getsym() != ident) error(14);
      pos = position();
      if (table[pos].kind != proc) error(15);
      gen(CAL, lev - table[pos].level, table[pos].adr);
      getsym();
      break;
    case ifsym:
      getsym();
      condition(lev);
      if (lex_sym != thensym) error(16);
      cx1 = cx;
      gen(JPC, 0, 0);
      getsym();
      statement(lev);
      code[cx1].a = cx;
      break;
    case beginsym:
      getsym();
      statement(lev);
      while (lex_sym != endsym) {
        if (lex_sym != semicolon) error(10);
        getsym();
        statement(lev);
      }
      getsym();
      break;
    case whilesym:
      cx1 = cx;
      getsym();
      condition(lev);
      cx2 = cx;
      gen(JPC, 0, 0);
      if (lex_sym != dosym) error(18);
      getsym();
      statement(lev);
      gen(JMP, 0, cx1);
      code[cx2].a = cx;
      break;
    default: break;
  }
}

static void block(int lev)
{
  int tx0, cx0;
  dx = 3;
  tx0 = tx;
  table[tx].adr = cx;
  gen(JMP, 0, 0);
  if (getsym() == constsym) constdeclaration(lev);
  if (lex_sym == varsym) vardeclaration(lev);
  while (lex_sym == procsym) {
    if (getsym() != ident) error(29);
    enter(proc, lev);
    if (getsym() != semicolon) error(4);
    block(lev + 1);
    if (lex_sym != semicolon) error(4);
    getsym();
  }
  table[tx0].adr = cx;    // start address of code
  cx0 = 0;
  gen(INT, 0, dx);
  statement(lev);
  gen(OPR, 0, 0);    // return
  listcode(cx0);
}

static void lex_init(void)
{
  lex_ch = 0x20;
  lex_line = lex_column = lex_num = 0;
  lex_str[0] = '\0';
  lex_sym = nil;
}

int main (int argc, char **argv)
{
  in = stdin; out = stdout; err = stderr;
  setbuf(out, NULL);
  lex_init();
  tx = 0, cx = 0;
  block(0);
  if (lex_sym != period) error(9);
  return 0;
}
