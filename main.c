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

#define MAX_IDENTIFIER 10
#define MAX_TX 100
#define MAX_CX 2000
#define STACK_SIZE 500

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

enum object_type { constant, variable, proc };
char *object_type_name[] = { "constant", "variable", "proc" };

enum mnemonic {
  LIT,    // [LIT, 0, a] load constant a
  OPR,    // [OPR, 0, a] execute operation a
  LOD,    // [LOD, l, a] load variable l, a
  STO,    // [STO, l, a] store variable l, a
  CAL,    // [CAL, l, a] call procedure a at level l
  INT,    // [INT, 0, a] increment t-regeister by a
  JMP,    // [JMP, 0, a] jump to a
  JPC     // [JPC, 0, a] jump conditional to a
};
char *mnemonic_name[] = {
  "LIT", "OPR", "LOD", "STO", "CAL", "INT", "JMP", "JPC"
};

struct instruction {
  enum mnemonic m;
  int l;
  int a;
};
static struct instruction code[MAX_CX];

struct object {
  char name[MAX_IDENTIFIER];
  enum object_type kind;
  int val;
  int level;
  int adr;
};
static struct object table[MAX_TX];

static FILE *in, *out, *err;

static int lex_ch, lex_line, lex_column;
static enum symbol lex_sym;
static int lex_num;
static char lex_str[MAX_IDENTIFIER];

static int tx, dx, cx;    // index of table, data allocation, code allocation

static void dump(void)
{
#ifndef NDEBUG 
  fprintf(out, "\n*** table ***\n");
  fprintf(out, "name\tobject_type\tlevel\taddress\tvalue\n");
  for (int i = 0; i < tx; i++)
    fprintf(out, "%s\t%s\t%d\t%d\t%d\n", table[i].name
        , object_type_name[table[i].kind], table[i].level , table[i].adr
        , table[i].val);
  fprintf(out, "\n*** instruction ***\n");
  for (int i = 0; i < cx; i++)
    fprintf(out, "%4d: %s %d,%d\n", i, mnemonic_name[code[i].m], code[i].l
        , code[i].a);
#endif
}

static void error(int n)
{
  char* msg = NULL;
  switch (n) {
    case 1: msg = "use '=' instead of ':='"; break;
    case 2: msg = "'=' must be followed by a number"; break;
    case 3: msg = "identifier must be followed by '='"; break;
    case 4: msg = "';' missing"; break;
    case 5: msg = "',' or ';' missing"; break;
    case 6: msg = "'.' expected"; break;
    case 10: msg = "';' between statements missing"; break;
    case 11: msg = "undeclared identifier"; break;
    case 12: msg = "assignment to constant or procedure is not allowed"; break;
    case 13: msg = "':=' expected"; break;
    case 14: msg = "call must be followed by an identifie"; break;
    case 15: msg = "call of a constant or variable is meaningless"; break;
    case 16: msg = "'then' expecte"; break;
    case 17: msg = "';' or 'end' expecte"; break;
    case 18: msg = "'do' expecte"; break;
    case 20: msg = "relational operator expected"; break;
    case 21: msg = "expression must not contain a procedure identifier"; break;
    case 22: msg = "')' missing"; break;
    case 23: msg = "source code is too large"; break;
    case 24: msg = "this identifier is too large"; break;
    case 25: msg = "eof reached"; break;
    case 26: msg = "factor expected"; break;
    case 27: msg = "'const' must be followed by identifier"; break;
    case 28: msg = "'var' must be followed by identifier"; break;
    case 29: msg = "'procedure' must be followed by identifier"; break;
    case 30: msg = "undefined operator"; break;
    case 31: msg = "undefined mnemonic"; break;
    case 32: msg = "object is too many"; break;
    case 33: msg = "undefined object type"; break;
    case 34: msg = "cannot find object"; break;
    default: msg = "error";
  }
  fprintf(err, "\nerror: %s.\n", msg);
  fprintf(err, "at line %d, column %d\n", lex_line + 1, lex_column + 1);
  dump();
  exit(1);
}

void nextch(void)
{
  if (feof(in)) error(25);
#ifndef NDEBUG
  if (lex_line == 0 && lex_ch == '\n')
    fprintf(out, "\n*** source ***\n");
  if (lex_ch == '\n') fprintf(out, "%4d: ", lex_line + 1);
#endif
  if ((lex_ch = fgetc(in)) != '\n') lex_column++;
  else {
    lex_line++;
    lex_column = 0;
  }
#ifndef NDEBUG
  fprintf(out, "%c", lex_ch);
#endif
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

static void gen(enum mnemonic m, int l, int a)
{
  if (cx > MAX_CX) error(23);
  code[cx].m = m;
  code[cx].l = l;
  code[cx].a = a;
  cx++;
}

static void enter(enum object_type o, int lev)
{
  if (tx > MAX_TX) error(32);
  strcpy(table[tx].name, lex_str);
  table[tx].kind = o;
  table[tx].val = table[tx].level = table[tx].adr = 0;
  switch (o) {
    case constant:
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

static struct object find_by_name(char *name)
{
  for (int i = tx; i >= 0; i--)
    if (strcmp(table[i].name, name) == 0) return table[i];
  error(11);
  return table[0];    // unreachable
}

static struct object find_by_adr(int adr)
{
  for (int i = 0; i < tx; i++)
    if (table[i].adr == adr) return table[i];
  error(34);
  return table[0];    // unreachable
}

static void parse_expression(int lev);

static void parse_factor(int lev)
{
  switch (lex_sym) {
    case ident:
      {
        struct object o = find_by_name(lex_str);
        switch (o.kind) {
          case constant: gen(LIT, 0, o.val); break;
          case variable:
            gen(LOD, lev - o.level, o.adr);
            break;
          case proc: error(21); break;
          default: error(33);
        }
        getsym();
        break;
      }
    case number:
      gen(LIT, 0, lex_num);
      getsym();
      break;
    case lparen:
      getsym();    // skip '('
      parse_expression(lev);
      if (lex_sym != rparen) error(22);
      getsym();    // skip ')'
      break;
    default: error(26);
  }
}

static void parse_term(int lev)
{
  enum symbol mulopp;
  parse_factor(lev);
  while (lex_sym == times || lex_sym == slash) {
    mulopp = lex_sym;
    getsym();
    parse_factor(lev);
    if (mulopp == times) gen(OPR, 0, 4);
    else gen(OPR, 0, 5);
  }
}

static void parse_expression(int lev)
{
  enum symbol addop;
  if (lex_sym == plus || lex_sym == minus) {
    addop = lex_sym;
    getsym();
    parse_term(lev);
    if (addop == minus) gen(OPR, 0, 1);
  } else  parse_term(lev);
  while (lex_sym == plus || lex_sym == minus) {
    addop = lex_sym;
    getsym();
    parse_term(lev);
    if (addop == plus) gen(OPR, 0, 2);
    else gen(OPR, 0, 3);
  }
}

static void parse_condition(int lev)
{
  enum symbol relop;
  if (lex_sym == oddsym) {
    getsym();
    parse_expression(lev);
    gen(OPR, 0, 6);
  } else {
    parse_expression(lev);
    relop = lex_sym;
    switch (relop) {
      case eql: case neq: case lss: case leq: case gtr: case geq: break;
      default: error(20);
    }
    getsym();
    parse_expression(lev);
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

static void parse_statement(int lev)
{
  struct object o;
  int cx1, cx2;
  switch (lex_sym) {
    case ident:
      o = find_by_name(lex_str);
      if (o.kind != variable) error(12);
      if (getsym() != becomes) error(13);
      getsym();    // skip ':='
      parse_expression(lev);
      gen(STO, lev - o.level, o.adr);
      break;
    case callsym:
      if (getsym() != ident) error(14);
      o = find_by_name(lex_str);
      if (o.kind != proc) error(15);
      gen(CAL, lev - o.level, o.adr);
      getsym();
      break;
    case ifsym:
      getsym();    // skip 'if'
      parse_condition(lev);
      if (lex_sym != thensym) error(16);
      cx1 = cx;
      gen(JPC, 0, 0);
      getsym();    // skip 'then'
      parse_statement(lev);
      code[cx1].a = cx;
      break;
    case beginsym:
      getsym();    // skip 'begin'
      parse_statement(lev);
      while (lex_sym != endsym) {
        if (lex_sym != semicolon) error(10);
        getsym();    // skip ';'
        parse_statement(lev);
        if (lex_sym != semicolon && lex_sym != endsym) error(17);
      }
      getsym();    // skip 'end'
      break;
    case whilesym:
      cx1 = cx;
      getsym();    // skip 'while'
      parse_condition(lev);
      cx2 = cx;
      gen(JPC, 0, 0);
      if (lex_sym != dosym) error(18);
      getsym();    //  skip 'do'
      parse_statement(lev);
      gen(JMP, 0, cx1);
      code[cx2].a = cx;
      break;
    default: break;
  }
}

static void parse_block(int lev)
{
  int tx0 = tx;
  dx = 3;
  table[tx].adr = cx;
  gen(JMP, 0, 0);
  if (getsym() == constsym) {
    while (lex_sym != semicolon) {
      if (getsym() != ident) error(27);
      if (getsym() == becomes) error(1);
      if (lex_sym != eql) error(3);
      if (getsym() != number) error(2);
      enter(constant, lev);
      if (getsym() != comma && lex_sym != semicolon) error(5);
    }
    getsym();    // skip ';'
  }
  if (lex_sym == varsym) {
    while (lex_sym != semicolon) {
      if (getsym() != ident) error(28);
      enter(variable, lev);
      if (getsym() != comma && lex_sym != semicolon) error(5);
    }
    getsym();    // skip ';'
  }
  while (lex_sym == procsym) {
    if (getsym() != ident) error(29);
    enter(proc, lev);
    if (getsym() != semicolon) error(4);
    parse_block(lev + 1);
    if (lex_sym != semicolon) error(4);
    getsym();
  }
  code[table[tx0].adr].a = cx;    // start address of code
  table[tx0].adr = cx;
  gen(INT, 0, dx);
  parse_statement(lev);
  gen(OPR, 0, 0);    // return
}

static void parse_program(void)
{
  parse_block(0);
  if (lex_sym != period) error(6);
}

static int base(int s[], int l, int b)
{
  int b1 = b;
  while (l-- > 0) b1 = s[b1];
  return b1;
}

static void interpret(void)
{
  struct instruction inst;    // instruction register
  int s[STACK_SIZE];    // stack
  int p, b, t;    // program, base, topstack-registers
  s[0] = s[1] = s[2] = 0;
  p = t = 0, b = 1;
  fprintf(out, "\n*** start xpl0 ***\n");
  do {
    inst = code[p++];
    switch (inst.m) {
      case LIT: s[++t] = inst.a; break;
      case OPR:
        switch (inst.a) {
          case 0: t = b - 1; p = s[t + 3]; b = s[t + 2]; break;
          case 1: s[t] = -s[t]; break;
          case 2: t--; s[t] = (s[t] + s[t + 1]); break;
          case 3: t--; s[t] = (s[t] - s[t + 1]); break;
          case 4: t--; s[t] = (s[t] * s[t + 1]); break;
          case 5: t--; s[t] = (s[t] / s[t + 1]); break;
          case 6: s[t] = (s[t] % 2 == 1); break;
          case 8: t--; s[t] = (s[t] == s[t + 1]); break;
          case 9: t--; s[t] = (s[t] != s[t + 1]); break;
          case 10: t--; s[t] = (s[t] < s[t + 1]); break;
          case 11: t--; s[t] = (s[t] >= s[t + 1]); break;
          case 12: t--; s[t] = (s[t] > s[t + 1]); break;
          case 13: t--; s[t] = (s[t] <= s[t + 1]); break;
          default: error(30);
        }
        break;
      case LOD: s[++t] = s[base(s, inst.l, b) + inst.a]; break;
      case STO:
        s[base(s, inst.l, b) + inst.a] = s[t];
        fprintf(out, "assign %d to %s\n", s[t], find_by_adr(inst.a).name);
        t--;
        break;
      case CAL:
        s[t + 1] = base(s, inst.l, b);
        s[t + 2] = b;
        s[t + 3] = p;
        b = t + 1;
        p = inst.a;
        break;
      case INT: t += inst.a; break;
      case JMP: p = inst.a; break;
      case JPC: if(s[t]) p = inst.a; else t--; break;
      default: error(31);
    }
  } while (p != 0);
}

static void lex_init(void)
{
  lex_ch = 0xA;
  lex_line = lex_column = lex_num = 0;
  lex_str[0] = '\0';
  lex_sym = nil;
}

int main (int argc, char **argv)
{
  in = stdin; out = stdout; err = stderr;
  setbuf(out, NULL);
  lex_init();
  tx = dx = cx = 0;
  parse_program();
  dump();
  interpret();
  return 0;
}
