// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "tools/alive_parser.h"
#include "ir/value.h"
#include "tools/alive_lexer.h"
#include "util/compiler.h"
#include <cassert>
#include <iostream>
#include <string>
#include <string_view>

#define YYDEBUG 0

using namespace IR;
using namespace tools;
using namespace std;

static_assert(LEXER_READ_AHEAD == PARSER_READ_AHEAD);

namespace tools {

static void error(string &&s) {
  throw ParseException(move(s), yylineno);
}

static unordered_map<string, Value*> identifiers;
static Function *fn;

static Value& get_constant(uint64_t n, Type *t) {
  auto c = make_unique<IntConst>(t->dup(), n);
  auto ret = c.get();
  fn->addConstant(move(c));
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

static void parse_pre(Transform &t) {
  if (!tokenizer.consumeIf(PRE))
    return;
  // TODO
}

static void parse_comma() {
  tokenizer.ensure(COMMA);
}

static unique_ptr<Type> parse_type(bool optional = true) {
  switch (auto t = *tokenizer) {
  case INT_TYPE:
    return make_unique<IntType>(yylval.num);
  default:
    if (optional) {
      tokenizer.unget(t);
      return make_unique<SymbolicType>();
    } else {
      error(string("Expecting a type, got: ") + token_name[t]);
    }
  }
  UNREACHABLE();
}

static Value& parse_operand(Type *type) {
  switch (auto t = *tokenizer) {
  case NUM:
    // FIXME: constraint type to int
    return get_constant(yylval.num, type);
  case TRUE:
    // FIXME: constrain types below to boolean
    return get_constant(1, type);
  case FALSE:
    return get_constant(0, type);
  case IDENTIFIER: {
    string id(yylval.str);
    if (auto I = identifiers.find(id); 
        I != identifiers.end())
      return *I->second;

    auto input = make_unique<Input>(type->dup(), string(id));
    auto ret = input.get();
    fn->addInput(move(input));
    identifiers.emplace(move(id), ret);
    return *ret;
  }
  default:
    error(string("Expected an operand, got: ") + token_name[t]);
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
  case AND:
  case OR:
  case XOR:
    return BinOp::None;
  default:
    UNREACHABLE();
  }
}

static unique_ptr<Instr> parse_binop(string_view name, token op_token) {
  BinOp::Flags flags = parse_binop_flags(op_token);
  auto type = parse_type();
  auto &a = parse_operand(type.get());
  parse_comma();
  auto &b = parse_operand(type.get());

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
  default:
    UNREACHABLE();
  }
  return make_unique<BinOp>(move(type), string(name), a, b, op, flags);
}

static unique_ptr<Instr> parse_conversionop(string_view name, token op_token) {
  // op ty %op to ty2
  auto opty = parse_type();
  auto &val = parse_operand(opty.get());
  unique_ptr<Type> ty2;
  if (tokenizer.consumeIf(TO)) {
    ty2 = parse_type(/*optional=*/false);
  }

  ConversionOp::Op op;
  switch (op_token) {
  case SEXT:  op = ConversionOp::SExt; break;
  case ZEXT:  op = ConversionOp::ZExt; break;
  case TRUNC: op = ConversionOp::Trunc; break;
  default:
    UNREACHABLE();
  }
  return make_unique<ConversionOp>(move(ty2), string(name), val, op);
}

static unique_ptr<Instr> parse_select(string_view name) {
  // select condty %cond, ty %a, ty %b
  auto condty = parse_type();
  auto &cond = parse_operand(condty.get());
  parse_comma();
  auto aty = parse_type();
  auto &a = parse_operand(aty.get());
  parse_comma();
  auto bty = parse_type();
  auto &b = parse_operand(bty.get());
  return make_unique<Select>(move(aty), string(name), cond, a, b);
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
  auto ty = parse_type();
  auto &a = parse_operand(ty.get());
  parse_comma();
  auto &b = parse_operand(ty.get());
  return make_unique<ICmp>(string(name), cond, a, b);
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
  case AND:
  case OR:
  case XOR:
    return parse_binop(name, t);
  case SEXT:
  case ZEXT:
  case TRUNC:
    return parse_conversionop(name, t);
  case SELECT:
    return parse_select(name);
  case ICMP:
    return parse_icmp(name);
  default:
    error(string("Expected instruction name; got: ") + token_name[t]);
  }
  UNREACHABLE();
}

static unique_ptr<Instr> parse_return() {
  auto type = parse_type();
  auto &val = parse_operand(type.get());
  return make_unique<Return>(move(type), val);
}

static void parse_fn(Function &f) {
  fn = &f;
  identifiers.clear();
  BasicBlock *bb = &f.getBB("");

  while (true) {
    switch (auto t = *tokenizer) {
    case IDENTIFIER: {
      string name(yylval.str);
      auto i = parse_instr(name);
      identifiers.emplace(move(name), i.get());
      bb->addIntr(move(i));
      break;
    }
    case LABEL:
      bb = &f.getBB(yylval.str);
      break;
    case RETURN:
      bb->addIntr(parse_return());
      break;
    case UNREACH:
      bb->addIntr(make_unique<Unreachable>());
      break;
    default:
      tokenizer.unget(t);
      return;
    }
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
    parse_name(t);
    parse_pre(t);
    parse_fn(t.src);
    parse_arrow();
    parse_fn(t.tgt);
  }

  identifiers.clear();
  return ret;
}

}
