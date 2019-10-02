// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "tools/alive_lexer.h"
#include "util/compiler.h"
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>

using namespace std;

#define YYCTYPE  unsigned char
#define YYCURSOR yycursor
#define YYLIMIT  yylimit
#define YYTEXT   yytext
#define YYMARKER yymarker
#define YYLENGTH ((size_t)(YYCURSOR - YYTEXT))
#define YYFILL(n) do { if ((YYCURSOR + n) >= (YYLIMIT + YYMAXFILL)) \
                         { return END; } } while (0)

static const YYCTYPE *YYCURSOR;
static const YYCTYPE *YYLIMIT;
static const YYCTYPE *YYTEXT;
static const YYCTYPE *YYMARKER;
static const YYCTYPE *tag1, *yyt1;

#if 0
# define YYRESTART() cout << "restart line: " << yylineno << '\n'; goto restart
# define YYDEBUG(s, c) cout << "state: " << s << " char: " << c << '\n'
#else
# define YYRESTART() goto restart
# define YYDEBUG(s, c)
#endif

/*!max:re2c */
static_assert(YYMAXFILL <= tools::LEXER_READ_AHEAD);

namespace tools {

unsigned yylineno;
yylval_t yylval;

const char *const token_name[] = {
#define TOKEN(x) #x,
#include "tools/tokens.h"
#undef TOKEN
};

static void error(string &&str) {
  throw LexException("[Lex] " + move(str), yylineno);
}

static void COPY_STR(unsigned off = 0) {
  assert(off <= YYLENGTH);
  yylval.str = { (const char*)YYTEXT + off, YYLENGTH - off };
}

static void COPY_STR_RTRIM(unsigned trim) {
  assert(trim <= YYLENGTH);
  yylval.str = { (const char*)YYTEXT, YYLENGTH - trim };
}

void yylex_init(string_view str) {
  YYCURSOR = (const YYCTYPE*)str.data();
  YYLIMIT  = (const YYCTYPE*)str.data() + str.size();
  yylineno = 1;
}

token yylex() {
restart:
  if (YYCURSOR >= YYLIMIT)
    return END;
  YYTEXT = YYCURSOR;

/*!re2c
space = [ \t];
re2c:yyfill:check = 0;

"\r"? "\n" {
  ++yylineno;
  YYRESTART();
}

space+ {
  YYRESTART();
}

";" [^\r\n]* {
  YYRESTART();
}

"Name:" [ \t]* @tag1 [^\r\n]+ {
  COPY_STR(tag1 - YYTEXT);
  return NAME;
}

"Pre:" {
  return PRE;
}

"i" [1-9][0-9]* {
  yylval.num = strtoull((char*)YYTEXT+1, nullptr, 10);
  return INT_TYPE;
}

"<" space* @tag1 [1-9][0-9]* space* "x" {
  yylval.num = strtoull((char*)tag1, nullptr, 10);
  return VECTOR_TYPE_PREFIX;
}

[-+]? [0-9]* "." [0-9]+ ([eE] [-+]? [0-9]+)? {
  yylval.fp_num = strtod((char*)YYTEXT, nullptr);
  return FP_NUM;
}

"-"?[0-9]+ {
  yylval.num = strtoull((char*)YYTEXT, nullptr, 10);
  if (yylval.num == ULLONG_MAX && errno == ERANGE) {
    COPY_STR();
    return NUM_STR;
  }
  return NUM;
}

"%" [a-zA-Z0-9_.]+ {
  COPY_STR();
  return REGISTER;
}

"@" [a-zA-Z0-9_.]+ {
  COPY_STR();
  return GLOBAL_NAME;
}

"C" [0-9]+ {
  COPY_STR();
  return CONSTANT;
}

"=" {
  return EQUALS;
}

"," {
  return COMMA;
}

"=>" {
  return ARROW;
}

[a-zA-Z]+ ":" {
  COPY_STR_RTRIM(1);
  return LABEL;
}

"("  { return LPAREN; }
")"  { return RPAREN; }
"+"  { return PLUS; }
"*"  { return STAR; }
"&&" { return BAND; }
"||" { return BOR; }
"=="  { return CEQ; }
"!="  { return CNE; }
">"  { return CSGT; }
"<"  { return CSLT; }
">u" { return CUGT; }
"<u" { return CULT; }

"true" { return TRUE; }
"false" { return FALSE; }
"undef" { return UNDEF; }
"poison" { return POISON; }
"null" { return NULLTOKEN; }
"add" { return ADD; }
"mul" { return MUL; }
"sub" { return SUB; }
"sdiv" { return SDIV; }
"udiv" { return UDIV; }
"srem" { return SREM; }
"urem" { return UREM; }
"shl" { return SHL; }
"ashr" { return ASHR; }
"lshr" { return LSHR; }
"sadd_sat" { return SADD_SAT; }
"uadd_sat" { return UADD_SAT; }
"ssub_sat" { return SSUB_SAT; }
"usub_sat" { return USUB_SAT; }
"and" { return AND; }
"or" { return OR; }
"xor" { return XOR; }
"nsw" { return NSW; }
"nuw" { return NUW; }
"exact" { return EXACT; }
"bitcast" { return BITCAST; }
"sext" { return SEXT; }
"zext" { return ZEXT; }
"trunc" { return TRUNC; }
"to" { return TO; }
"select" { return SELECT; }
"icmp" { return ICMP; }
"eq" { return EQ; }
"ne" { return NE; }
"sle" { return SLE; }
"slt" { return SLT; }
"sge" { return SGE; }
"sgt" { return SGT; }
"ule" { return ULE; }
"ult" { return ULT; }
"uge" { return UGE; }
"ugt" { return UGT; }
"call" { return CALL; }
"freeze" { return FREEZE; }
"malloc" { return MALLOC; }
"free" { return FREE; }
"fshl" { return FSHL; }
"fshr" { return FSHR; }
"ret" { return RETURN; }
"bswap" { return BSWAP; }
"bitreverse" { return BITREVERSE; }
"cttz" { return CTTZ; }
"ctlz" { return CTLZ; }
"ctpop" { return CTPOP; }
"extractvalue" { return EXTRACTVALUE; }
"sadd_overflow" { return SADD_OVERFLOW; }
"uadd_overflow" { return UADD_OVERFLOW; }
"ssub_overflow" { return SSUB_OVERFLOW; }
"usub_overflow" { return USUB_OVERFLOW; }
"smul_overflow" { return SMUL_OVERFLOW; }
"umul_overflow" { return UMUL_OVERFLOW; }
"fadd" { return FADD; }
"fsub" { return FSUB; }
"fmul" { return FMUL; }
"fdiv" { return FDIV; }
"frem" { return FREM; }
"fcmp" { return FCMP; }
"fneg" { return FNEG; }
"oeq" { return OEQ; }
"ogt" { return OGT; }
"oge" { return OGE; }
"olt" { return OLT; }
"ole" { return OLE; }
"one" { return ONE; }
"ord" { return ORD; }
"ueq" { return UEQ; }
"une" { return UNE; }
"uno" { return UNO; }
"fptosi" { return FPTOSI; }
"fptoui" { return FPTOUI; }
"sitofp" { return SITOFP; }
"uitofp" { return UITOFP; }
"ptrtoint" { return PTRTOINT; }
"half" { return HALF;}
"float" { return FLOAT;}
"double" { return DOUBLE;}
"nnan" { return NNAN; }
"ninf" { return NINF; }
"unreachable" { return UNREACH; }

[a-zA-Z][a-zA-Z0-9]* {
  COPY_STR();
  return IDENTIFIER;
}

* { error("couldn't parse: '" + string((char*)YYTEXT, 16) + '\''); }

*/

  UNREACHABLE();
}

}
