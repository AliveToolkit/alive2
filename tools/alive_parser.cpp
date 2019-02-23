// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "tools/alive_parser.h"
#include "ir/constant.h"
#include "ir/value.h"
#include "tools/alive_lexer.h"
#include "util/compiler.h"
#include <cassert>
#include <memory>
#include <unordered_map>
#include <vector>

#define YYDEBUG 0

#if YYDEBUG
#include <iostream>
#endif

using namespace IR;
using namespace tools;
using namespace std;

static_assert(LEXER_READ_AHEAD == PARSER_READ_AHEAD);

namespace tools {

static void error(string &&s) {
  throw ParseException(move(s), yylineno);
}

static void error(const char *s, token t) {
  throw ParseException(string(s) + "; got: " + token_name[t], yylineno);
}

static vector<unique_ptr<IntType>> int_types;
static unordered_map<string, Value*> identifiers;
static Function *fn;

static Value& get_constant(uint64_t n, Type &t) {
  auto c = make_unique<IntConst>(t, n);
  auto ret = c.get();
  fn->addConstant(move(c));
  return *ret;
}

static Value& get_num_constant(string_view n, Type &t) {
  auto c = make_unique<IntConst>(t, string(n));
  auto ret = c.get();
  fn->addConstant(move(c));
  return *ret;
}

static Value& get_constant(string_view name, Type &type) {
  string id(name);
  if (auto I = identifiers.find(id);
      I != identifiers.end())
    return *I->second;

  auto c = make_unique<ConstantInput>(type, string(id));
  auto ret = c.get();
  fn->addConstant(move(c));
  identifiers.emplace(move(id), ret);
  return *ret;
}


struct tokenizer_t {
  token last;
  // true if token last was 'unget' and should be returned next
  bool returned = false;

  token operator*() {
    if (returned) {
      returned = false;
      return last;
    }
    return get_new_token();
  }

  token peek() {
    if (returned)
      return last;
    returned = true;
    return last = get_new_token();
  }

  bool consumeIf(token expected) {
    auto token = peek();
    if (token == expected) {
      returned = false;
      return true;
    }
    return false;
  }

  void ensure(token expected) {
    auto t = **this;
    if (t != expected)
      error(string("expected token: ") + token_name[expected] + ", got: " +
            token_name[t]);
  }

  void unget(token t) {
    assert(returned == false);
    returned = true;
    last = t;
  }

  bool empty() {
    return peek() == END;
  }

  bool isType() {
    return peek() == INT_TYPE;
  }

private:
  token get_new_token() const {
    try {
      auto t = yylex();
#if YYDEBUG
      cout << "token: " << token_name[t] << '\n';
#endif
      return t;
    } catch (LexException &e) {
      throw ParseException(move(e.str), e.lineno);
    }
  }
};

static tokenizer_t tokenizer;


static void parse_name(Transform &t) {
  if (tokenizer.consumeIf(NAME))
    t.name = yylval.str;
}

static Value& parse_const_expr(Type &type);

static Predicate* parse_predicate(Predicate *last = nullptr,
                                  token last_t = END) {
  switch (auto t = *tokenizer) {
  case LPAREN: {
    auto newpred = parse_predicate();
    tokenizer.ensure(RPAREN);
    if (last == nullptr) {
parse_more:
      switch (auto t = *tokenizer) {
      case BAND:
      case BOR:
        return parse_predicate(newpred, t);
      default:
        tokenizer.unget(t);
        return newpred;
      }
    }

    if (last_t == BAND) {
      auto p = make_unique<BoolPred>(*last, *newpred, BoolPred::AND);
      newpred = p.get();
      fn->addPredicate(move(p));
      goto parse_more;
    }

    assert(last_t == BOR);

    switch (auto t = *tokenizer) {
    case BAND: {
      newpred = parse_predicate(newpred, BAND);
      auto p = make_unique<BoolPred>(*last, *newpred, BoolPred::OR);
      auto ret = p.get();
      fn->addPredicate(move(p));
      return ret;
    }

    case BOR: {
      auto p = make_unique<BoolPred>(*last, *newpred, BoolPred::OR);
      auto ret = p.get();
      fn->addPredicate(move(p));
      return parse_predicate(ret, BOR);
    }

    default: {
      tokenizer.unget(t);
      auto p = make_unique<BoolPred>(*last, *newpred,
                                     last_t == BAND ? BoolPred::AND :
                                                      BoolPred::OR);
      auto ret = p.get();
      fn->addPredicate(move(p));
      return ret;
    }
    }
    break;
  }
  case IDENTIFIER:
    //TODO
    break;

  case CONSTANT:
    //TODO
    parse_const_expr(*int_types[64].get());
    break;

  default:
    error("Expected predicate", t);
  }
  UNREACHABLE();
}

static void parse_pre(Transform &t) {
  if (!tokenizer.consumeIf(PRE))
    return;
  parse_predicate();
}

static void parse_comma() {
  tokenizer.ensure(COMMA);
}


static vector<unique_ptr<SymbolicType>> sym_types;
static unsigned sym_num;

static Type& parse_type(bool optional = true) {
  switch (auto t = *tokenizer) {
  case INT_TYPE:
    if (yylval.num > 4 * 1024)
      error("Int type too long: " + to_string(yylval.num));

    if (yylval.num >= int_types.size())
      int_types.resize(yylval.num + 1);

    if (!int_types[yylval.num])
      int_types[yylval.num] =
        make_unique<IntType>("i" + to_string(yylval.num), yylval.num);

    return *int_types[yylval.num].get();

  default:
    if (optional) {
      tokenizer.unget(t);
      if (sym_num < sym_types.size())
        return *sym_types[sym_num++].get();

      auto t = make_unique<SymbolicType>("symty_" + to_string(sym_num++));
      return *sym_types.emplace_back(move(t)).get();
    } else {
      error("Expecting a type", t);
    }
  }
  UNREACHABLE();
}

static Type& try_parse_type(Type &default_type) {
  if (tokenizer.isType())
    return parse_type();
  return default_type;
}

static Value& parse_operand(Type &type);

static Value& parse_const_expr(Type &type) {
  switch (auto t = *tokenizer) {
  case LPAREN: {
    auto &ret = parse_const_expr(type);
    tokenizer.ensure(RPAREN);
    return ret;
  }
  case CONSTANT:
    return get_constant(yylval.str, type);
  case IDENTIFIER: {
    string_view name = yylval.str;
    std::vector<Value*> args;
    tokenizer.ensure(LPAREN);
    if (!tokenizer.consumeIf(RPAREN)) {
      do {
        args.push_back(&parse_operand(parse_type()));
      } while (tokenizer.consumeIf(COMMA));
      tokenizer.ensure(RPAREN);
    }
    try {
      auto f = make_unique<ConstantFn>(type, name, move(args));
      auto ret = f.get();
      fn->addConstant(move(f));
      return *ret;
    } catch (ConstantFnException &e) {
      error(move(e.str));
    }
  }
  default:
    error("Expected constant expression", t);
  }
  UNREACHABLE();
}

static Value& parse_operand(Type &type) {
  switch (auto t = *tokenizer) {
  case NUM:
    return get_constant(yylval.num, type);
  case NUM_STR:
    return get_num_constant(yylval.str, type);
  case TRUE:
    return get_constant(1, type);
  case FALSE:
    return get_constant(0, type);
  case UNDEF: {
    auto val = make_unique<UndefValue>(type);
    auto ret = val.get();
    fn->addUndef(move(val));
    return *ret;
  }
  case POISON: {
    auto val = make_unique<PoisonValue>(type);
    auto ret = val.get();
    fn->addConstant(move(val));
    return *ret;
  }
  case REGISTER: {
    string id(yylval.str);
    if (auto I = identifiers.find(id);
        I != identifiers.end())
      return *I->second;

    auto input = make_unique<Input>(type, string(id));
    auto ret = input.get();
    fn->addInput(move(input));
    identifiers.emplace(move(id), ret);
    return *ret;
  }
  case CONSTANT:
  case IDENTIFIER:
  case LPAREN:
    tokenizer.unget(t);
    return parse_const_expr(type);
  default:
    error("Expected an operand", t);
  }
  UNREACHABLE();
}

static BinOp::Flags parse_nsw_nuw() {
  BinOp::Flags flags = BinOp::None;
  while (true) {
    if (tokenizer.consumeIf(NSW)) {
      flags = (BinOp::Flags)(flags | BinOp::NSW);
    } else if (tokenizer.consumeIf(NUW)) {
      flags = (BinOp::Flags)(flags | BinOp::NUW);
    } else {
      break;
    }
  }
  return flags;
}

static BinOp::Flags parse_exact() {
  if (tokenizer.consumeIf(EXACT))
    return BinOp::Exact;
  return BinOp::None;
}

static BinOp::Flags parse_binop_flags(token op_token) {
  switch (op_token) {
  case ADD:
  case SUB:
  case MUL:
  case SHL:
    return parse_nsw_nuw();
  case SDIV:
  case UDIV:
  case LSHR:
  case ASHR:
    return parse_exact();
  case SREM:
  case UREM:
  case UADD_SAT:
  case SADD_SAT:
  case USUB_SAT:
  case SSUB_SAT:
  case AND:
  case OR:
  case XOR:
  case CTTZ:
  case CTLZ:
    return BinOp::None;
  default:
    UNREACHABLE();
  }
}

static unique_ptr<Instr> parse_binop(string_view name, token op_token) {
  BinOp::Flags flags = parse_binop_flags(op_token);
  auto &type = parse_type();
  auto &a = parse_operand(type);
  parse_comma();
  auto &type2 = try_parse_type(type);
  auto &b = parse_operand(type2);

  BinOp::Op op;
  switch (op_token) {
  case ADD:  op = BinOp::Add; break;
  case SUB:  op = BinOp::Sub; break;
  case MUL:  op = BinOp::Mul; break;
  case SDIV: op = BinOp::SDiv; break;
  case UDIV: op = BinOp::UDiv; break;
  case SREM: op = BinOp::SRem; break;
  case UREM: op = BinOp::URem; break;
  case SHL:  op = BinOp::Shl; break;
  case LSHR: op = BinOp::LShr; break;
  case ASHR: op = BinOp::AShr; break;
  case AND:  op = BinOp::And; break;
  case OR:   op = BinOp::Or; break;
  case XOR:  op = BinOp::Xor; break;
  case CTTZ: op = BinOp::Cttz; break;
  case CTLZ: op = BinOp::Ctlz; break;
  case SADD_SAT: op = BinOp::SAdd_Sat; break;
  case UADD_SAT: op = BinOp::UAdd_Sat; break;
  case SSUB_SAT: op = BinOp::SSub_Sat; break;
  case USUB_SAT: op = BinOp::USub_Sat; break;
  default:
    UNREACHABLE();
  }
  return make_unique<BinOp>(type, string(name), a, b, op, flags);
}

static unique_ptr<Instr> parse_unaryop(string_view name, token op_token) {
  UnaryOp::Op op;
  switch (op_token) {
  case BITREVERSE: op = UnaryOp::BitReverse; break;
  case BSWAP:      op = UnaryOp::BSwap; break;
  case CTPOP:      op = UnaryOp::Ctpop; break;
  default:
    UNREACHABLE();
  }

  auto &ty = parse_type();
  auto &a = parse_operand(ty);
  return make_unique<UnaryOp>(ty, string(name), a, op);
}

static unique_ptr<Instr> parse_conversionop(string_view name, token op_token) {
  // op ty %op to ty2
  auto &opty = parse_type();
  auto &val = parse_operand(opty);
  auto &ty2 = parse_type(/*optional=*/!tokenizer.consumeIf(TO));

  ConversionOp::Op op;
  switch (op_token) {
  case SEXT:  op = ConversionOp::SExt; break;
  case ZEXT:  op = ConversionOp::ZExt; break;
  case TRUNC: op = ConversionOp::Trunc; break;
  default:
    UNREACHABLE();
  }
  return make_unique<ConversionOp>(ty2, string(name), val, op);
}

static unique_ptr<Instr> parse_select(string_view name) {
  // select condty %cond, ty %a, ty %b
  auto &condty = parse_type();
  auto &cond = parse_operand(condty);
  parse_comma();
  auto &aty = parse_type();
  auto &a = parse_operand(aty);
  parse_comma();
  auto &bty = parse_type();
  auto &b = parse_operand(bty);
  return make_unique<Select>(aty, string(name), cond, a, b);
}

static ICmp::Cond parse_icmp_cond() {
  switch (auto t = *tokenizer) {
  case EQ:  return ICmp::EQ;
  case NE:  return ICmp::NE;
  case SLE: return ICmp::SLE;
  case SLT: return ICmp::SLT;
  case SGE: return ICmp::SGE;
  case SGT: return ICmp::SGT;
  case ULE: return ICmp::ULE;
  case ULT: return ICmp::ULT;
  case UGE: return ICmp::UGE;
  case UGT: return ICmp::UGT;
  default:
    tokenizer.unget(t);
    return ICmp::Any;
  }
}

static unique_ptr<Instr> parse_icmp(string_view name) {
  // icmp cond ty %a, &b
  auto cond = parse_icmp_cond();
  auto &ty = parse_type();
  auto &a = parse_operand(ty);
  parse_comma();
  auto &b = parse_operand(ty);
  return make_unique<ICmp>(*int_types[1].get(), string(name), cond, a, b);
}

static unique_ptr<Instr> parse_freeze(string_view name) {
  // freeze ty %op
  auto &ty = parse_type();
  auto &op = parse_operand(ty);
  return make_unique<Freeze>(ty, string(name), op);
}

static unique_ptr<Instr> parse_copyop(string_view name, token t) {
  tokenizer.unget(t);
  auto &ty = parse_type();
  auto &op = parse_operand(ty);
  return make_unique<CopyOp>(ty, string(name), op);
}

static unique_ptr<Instr> parse_instr(string_view name) {
  // %name = instr arg1, arg2, ...
  tokenizer.ensure(EQUALS);
  switch (auto t = *tokenizer) {
  case ADD:
  case SUB:
  case MUL:
  case SDIV:
  case UDIV:
  case SREM:
  case UREM:
  case SHL:
  case ASHR:
  case LSHR:
  case SADD_SAT:
  case UADD_SAT:
  case SSUB_SAT:
  case USUB_SAT:
  case AND:
  case OR:
  case XOR:
  case CTTZ:
  case CTLZ:
    return parse_binop(name, t);
  case BITREVERSE:
  case BSWAP:
  case CTPOP:
    return parse_unaryop(name, t);
  case SEXT:
  case ZEXT:
  case TRUNC:
    return parse_conversionop(name, t);
  case SELECT:
    return parse_select(name);
  case ICMP:
    return parse_icmp(name);
  case FREEZE:
    return parse_freeze(name);
  case INT_TYPE:
  case NUM:
  case TRUE:
  case FALSE:
  case UNDEF:
  case POISON:
  case REGISTER:
    return parse_copyop(name, t);
  default:
    error("Expected instruction name", t);
  }
  UNREACHABLE();
}

static unique_ptr<Instr> parse_return() {
  auto &type = parse_type();
  auto &val = parse_operand(type);
  return make_unique<Return>(type, val);
}

static void parse_fn(Function &f) {
  fn = &f;
  identifiers.clear();
  BasicBlock *bb = &f.getBB("");
  bool has_return = false;

  while (true) {
    switch (auto t = *tokenizer) {
    case REGISTER: {
      string name(yylval.str);
      auto i = parse_instr(name);
      identifiers.emplace(move(name), i.get());
      bb->addInstr(move(i));
      break;
    }
    case LABEL:
      bb = &f.getBB(yylval.str);
      break;
    case RETURN: {
      auto instr = parse_return();
      f.setType(instr->getType());
      bb->addInstr(move(instr));
      has_return = true;
      break;
    }
    case UNREACH:
      bb->addInstr(make_unique<Assume>(get_constant(0, *int_types[1].get()),
                                       /*if_non_poison=*/false));
      break;
    default:
      tokenizer.unget(t);
      return;
    }
  }

  // FIXME: if target: copy relevant src instructions
  // FIXME: add error checking
  if (!has_return) {
    auto &last = bb->back();
    f.setType(last.getType());
  }
}

static void parse_arrow() {
  tokenizer.ensure(ARROW);
}

vector<Transform> parse(string_view buf) {
  vector<Transform> ret;

  yylex_init(buf);

  while (!tokenizer.empty()) {
    auto &t = ret.emplace_back();
    sym_num = 0;
    parse_name(t);
    parse_pre(t);
    parse_fn(t.src);
    parse_arrow();
    parse_fn(t.tgt);
  }

  identifiers.clear();
  return ret;
}


parser_initializer::parser_initializer() {
  int_types.resize(65);
  int_types[1] = make_unique<IntType>("i1", 1);
}

parser_initializer::~parser_initializer() {
  for_each(int_types.begin(), int_types.end(), [](auto &e) { e.reset(); });
  sym_types.clear();
}

}
