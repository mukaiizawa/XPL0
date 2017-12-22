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
#define RESERVED_WORDS 11
#define MAX_IDENTIFIER 10
#define MAX_ADDRESS 2047
#define MAX_LEVEL 3

// size of code array
#define CMAX 200

enum symbol {
  becomes, beginsym, callsym, comma, constsym, dosym, endsym, eql, geq, gtr,
  ident, ifsym, leq, lparen, lss, minus, neq, nil, number, oddsym, period, plus,
  procsym, rparen, semicolon, slash, thensym, times, varsym, whilesym
};

char *symbol_name[] = {
  ":=", "begin", "call", ",", "const", "do", "end", "=", "[", ">", "ident",
  "if", "[", "(", "<", "-", "#", "nil", "number", "odd", "period", "+",
  "procedure", ")", ";", "/", "then", "*", "var", "while"
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

char *mnemonic[] = { "LIT", "OPR", "LOD", "STO", "CAL", "INT", "JMP", "JPC" };

struct instruction {
  enum fct f;
  int l;
  int a;
};

struct table_record {
  char name[MAX_IDENTIFIER];
  enum object kind;
  int val;
  int level;
  int adr;
};

static char *word[RESERVED_WORDS];
static enum symbol wsym[RESERVED_WORDS];
static struct table_record table[TABLE_SIZE];

static FILE *in;
static FILE *out;
static FILE *err;

static enum symbol lex_sym;
static int lex_num;
static char lex_str[MAX_IDENTIFIER];
static int lex_line;
static int lex_column;

static char ch = 0x20;
static int tx = 0;    // table contents
static int dx = 0;
static int cx = 0;    // code allocation index
static struct instruction code[CMAX];


static void error(int n)
{
  char* msg = NULL;
  switch (n) {
    case 1: msg = "Use '=' instead of ':='"; break;
    case 2: msg = "'=' must be followed by a number"; break;
    case 3: msg = "Identifier must be followed by '='"; break;
    case 4: msg = "'const', 'var', 'procedure' must be followed by identifier";
            break;
    case 5: msg = "';' or ',' missing"; break;
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
    case 19: msg = "Incorrect symbol following statement"; break;
    case 20: msg = "Relational operator expected"; break;
    case 21: msg = "Expression must not contain a procedure identifier"; break;
    case 22: msg = "Right parenthesis missing"; break;
    case 23: msg = "An expression cannot begin with this symbol"; break;
    case 24: msg = "This Identifier is too large"; break;
    case 25: msg = "eof reached"; break;
    case 30: msg = "This number is too large"; break;
    default: msg = "error";
  }
  fprintf(err, "\nError: %s.\n", msg);
  fprintf(err, "at line: %d, column: %d\n", lex_line, lex_column);
  fprintf(err, "symbol: '%s', number: '%i', string: '%s'\n"
      , symbol_name[lex_sym], lex_num, lex_str);
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
  if ((ch = fgetc(in)) != '\n') lex_column++;
  else {
    lex_line++;
    lex_column = 0;
  }
}

void dump_table(void)
{
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

void getsym(void)
{
  while (isspace(ch)) nextch();
  if (isalpha(ch)) {
    lex_sym = ident;
    for (int i = 0; isalnum(ch); i++) {
      if ((i + 1) >= MAX_IDENTIFIER) error(24);
      lex_str[i] = ch;
      lex_str[i + 1] = '\0';
      nextch();
    }
    for (int i = 0; i < RESERVED_WORDS; i++)
      if (strcmp(lex_str, word[i]) == 0) {
        lex_sym = wsym[i];
        break;
      }
  } else if (isdigit(ch)) {
    lex_num = 0;
    lex_sym = number;
    while (isdigit(ch)) {
      lex_num = 10 * lex_num + (ch - '0');
      nextch();
    }
  } else {
    switch (ch) {
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
        if (ch == '=') {
          lex_sym = becomes;
          break;
        }
      default: lex_sym = nil;
    }
    nextch();
  }
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
  dump_table();
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
    getsym();
    if (lex_sym != ident) error(4);
    getsym();
    if (lex_sym == becomes) error(1);
    if (lex_sym != eql) error(3);
    getsym();
    if (lex_sym != number) error(2);
    enter(constant, lev);
    getsym();
    if (lex_sym != comma && lex_sym != semicolon) error(5);
  }
  getsym();
}

static void vardeclaration(int lev)
{
  while (lex_sym != semicolon) {
    getsym();
    if (lex_sym != ident) error(4);
    enter(variable, lev);
    getsym();
    if (lex_sym != comma && lex_sym != semicolon) error(5);
  }
  getsym();
}

void listcode(int cx0)
{
  for (int i = cx0; i < cx - 1; i++)
    fprintf(out, "%4d: %s %d,%d\n", i, mnemonic[code[i].f], 1, code[i].a);
}

static void expression(int lev);

void factor(int lev)
{
  while (lex_sym == ident || lex_sym == number || lex_sym == lparen) {
    switch (lex_sym) {
      case ident:
        {
          int pos;
          if ((pos = position()) == 0) error(11);
          switch (table[pos].kind) {
            case constant: gen(LIT, 0, table[pos].val);
            case variable: gen(LOD, lev - table[pos].level, table[pos].adr);
            case proc: error(21);
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
      default: error(-1);
    }
  }
}

void term(int lev)
{
  enum symbol mulopp;
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
  if (lex_sym != plus && lex_sym != minus) term(lev);
  else {
    addop = lex_sym;
    getsym();
    term(lev);
    if (addop == minus) gen(OPR, 0, 1);
  }
  while (lex_sym == plus || lex_sym == minus) {
    addop = lex_sym;
    getsym();
    term(lev);
    if (addop == plus) gen(OPR, 0, 2);
    else gen(OPR, 0, 3);
  }
}

void condition(int lev)
{
  enum symbol relop;
  if (lex_sym == oddsym) {
    getsym();
    expression(lev);
    gen(OPR, 0, 6);
  } else {
    expression(lev);
    switch (lex_sym) {
      case eql: case neq: case lss: case leq: case gtr: case geq: break;
      default: error(20);
    }
    relop = lex_sym;
    getsym();
    expression(lev);
    switch (relop) {
      case eql: gen(OPR, 0, 8); break;
      case neq: gen(OPR, 0, 9); break;
      case lss: gen(OPR, 0, 10); break;
      case geq: gen(OPR, 0, 11); break;
      case gtr: gen(OPR, 0, 12); break;
      case leq: gen(OPR, 0, 13); break;
      default: error(20);
    }
  }
}

void statement(int lev)
{
  int cx1, cx2;
  int pos = 0;
  switch (lex_sym) {
    case ident:
      pos = position();
      if (pos == 0) error(11);
      else if (table[pos].kind != variable) error(12);
      getsym();
      if (lex_sym == becomes) getsym();
      else error(13);
      expression(lev);
      gen(STO, lev - table[pos].level, table[pos].adr);
      break;
    case callsym:
      getsym();
      if (lex_sym != ident) error(14);
      pos = position();
      if (pos == 0) error(11);
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
      statement(lev);
      code[cx1].a = cx;
      break;
    case beginsym:
      getsym();
      statement(lev);
      while (lex_sym == semicolon
          || lex_sym == beginsym
          || lex_sym == callsym
          || lex_sym == ifsym
          || lex_sym == whilesym)
      {
        if (lex_sym != semicolon) error(10);
        getsym();
        statement(lev);
      }
      if (lex_sym != endsym) error(17);
      getsym();
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
    default: error(99);
  }
}

static void block(int lev)
{
  int dx, tx0, cx0;
  dx = 3;
  tx0 = tx;
  table[tx].adr = cx;
  gen(JMP, 0, 0);
  if (lev > MAX_LEVEL) error(32);
  getsym();
  if (lex_sym == constsym) constdeclaration(lev);
  if (lex_sym == varsym) vardeclaration(lev);
  if (lex_sym == procsym) {
    getsym();
    if (lex_sym != ident) error(4);
    enter(proc, lev);
    getsym();
    if (lex_sym != semicolon) error(5);
    block(lev + 1);
    getsym();
    if (lex_sym != semicolon) error(5);
  }
  table[tx0].adr = cx;    // start address of code
  cx0 = 0;
  gen(INT, 0, dx);
  // statement(lev);
  listcode(cx0);
}

static void lex_init(void)
{
  lex_line = lex_column = lex_num = 0;
  lex_str[0] = '\0';
  lex_sym = nil;
}

int main (int argc, char **argv)
{
  in = stdin;
  out = stdout;
  err = stderr;
  setbuf(out, NULL);
  lex_init();
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
  block(0);
  return 0;
}
