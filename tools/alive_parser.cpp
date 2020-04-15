// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "tools/alive_parser.h"
#include "ir/constant.h"
#include "ir/precondition.h"
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
static vector<unique_ptr<PtrType>> pointer_types;
static FloatType half_type("half", FloatType::Half);
static FloatType float_type("float", FloatType::Float);
static FloatType double_type("double", FloatType::Double);
static unordered_map<string, Value*> identifiers, identifiers_src;
static Function *fn;
static BasicBlock *bb;
static bool parse_src;

static Type& get_sym_type();
static Value& parse_operand(Type &type);

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
  fn->addInput(move(c));
  identifiers.emplace(move(id), ret);
  return *ret;
}

static Value& get_fp_constant(double n, Type &t) {
  auto c = make_unique<FloatConst>(t, n);
  auto ret = c.get();
  fn->addConstant(move(c));
  return *ret;
}

namespace {
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
    // TODO: support other aggregate types
    return isScalarType() || isVectorType() || isArrayType();
  }

  bool isScalarType() {
    return peek() == INT_TYPE || peek() == HALF ||
           peek() == FLOAT || peek() == DOUBLE || peek() == STAR;
  }

  bool isVectorType() {
    return peek() == VECTOR_TYPE_PREFIX;
  }

  bool isArrayType() {
    return peek() == ARRAY_TYPE_PREFIX;
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
}

static tokenizer_t tokenizer;


static void parse_name(Transform &t) {
  if (tokenizer.consumeIf(NAME))
    t.name = yylval.str;
}

static string parse_global_name() {
  tokenizer.ensure(GLOBAL_NAME);
  return string(yylval.str);
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
  case IDENTIFIER: {
    // function: identifier '(' arg0, ... argn ')'
    vector<Value*> args;
    auto name = yylval.str;
    tokenizer.ensure(LPAREN);
    while (true) {
      args.emplace_back(&parse_operand(get_sym_type()));

      if (!tokenizer.consumeIf(COMMA))
        break;
    }
    tokenizer.ensure(RPAREN);

    auto p = make_unique<FnPred>(name, move(args));
    auto ret = p.get();
    fn->addPredicate(move(p));
    return ret;
  }
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
  fn = &t.src;
  t.precondition = parse_predicate();
}

static void parse_comma() {
  tokenizer.ensure(COMMA);
}

static uint64_t parse_number() {
  tokenizer.ensure(NUM);
  return yylval.num;
}


static unordered_map<Type*, unique_ptr<StructType>> overflow_aggregate_types;
static vector<unique_ptr<SymbolicType>> sym_types;
static unsigned sym_num;
static unsigned struct_num;

static Type& get_overflow_type(Type &type) {
  auto p = overflow_aggregate_types.try_emplace(&type);
  auto &st = p.first->second;
  if (p.second)
    // TODO: Knowing exact layout of { i1, ity } needs data layout.
    st = make_unique<StructType>("structty_" + to_string(struct_num++),
           initializer_list<Type*>({ &type, int_types[1].get() }),
           initializer_list<bool>({ false, false }));
  return *st.get();
}

static Type& get_sym_type() {
  // NOTE: don't reuse sym_types across transforms or printing is messed up
  return *sym_types.emplace_back(
    make_unique<SymbolicType>("symty_" + to_string(sym_num++))).get();
}

static Type& get_int_type(unsigned size) {
  if (size >= int_types.size())
    int_types.resize(size + 1);

  if (!int_types[size])
    int_types[size] = make_unique<IntType>("i" + to_string(size), size);

  return *int_types[size].get();
}

static Type& get_pointer_type(unsigned address_space_number) {
  if (address_space_number >= pointer_types.size())
    pointer_types.resize(address_space_number + 1);

  if (!pointer_types[address_space_number])
    pointer_types[address_space_number] = make_unique<PtrType>(
        address_space_number);

  return *pointer_types[address_space_number].get();
}

static unsigned vector_num;
static vector<unique_ptr<VectorType>> vector_types;
static unsigned array_num;
static vector<unique_ptr<ArrayType>> array_types;

static Type& parse_scalar_type() {
  switch (*tokenizer) {
  case INT_TYPE:
    if (yylval.num > 4 * 1024)
      error("Int type too long: " + to_string(yylval.num));
    return get_int_type(yylval.num);

  case HALF:
    return half_type;

  case FLOAT:
    return float_type;

  case DOUBLE:
    return double_type;

  case STAR:
    // TODO: Pointer type of non-zero address space
    return get_pointer_type(0);

  default:
    UNREACHABLE();
  }
}

static Type& parse_vector_type() {
  tokenizer.ensure(VECTOR_TYPE_PREFIX);
  unsigned elements = yylval.num;

  Type &elemTy = parse_scalar_type();

  tokenizer.ensure(CSGT);
  return *vector_types.emplace_back(
    make_unique<VectorType>("vty_" + to_string(vector_num++),
                            elements, elemTy)).get();
}

static Type& parse_array_type();

static Type& parse_type(bool optional = true) {
  if (tokenizer.isScalarType()) {
    return parse_scalar_type();
  }

  if (tokenizer.isArrayType()) {
    return parse_array_type();
  }

  if (tokenizer.isVectorType()) {
    return parse_vector_type();
  }

  if (optional)
    return get_sym_type();
  else
    error("Expecting a type", tokenizer.peek());

  UNREACHABLE();
}

static Type& parse_array_type() {
  tokenizer.ensure(ARRAY_TYPE_PREFIX);
  unsigned elements = yylval.num;

  Type &elemTy = parse_type(false);
  tokenizer.ensure(RSQBRACKET);
  return *array_types.emplace_back(
          make_unique<ArrayType>("aty_" + to_string(array_num++),
                                  elements, elemTy)).get();
}

static Type& try_parse_type(Type &default_type) {
  if (tokenizer.isType())
    return parse_type();
  return default_type;
}

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
    vector<Value*> args;
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

static Value& get_or_copy_instr(const string &name) {
  assert(!parse_src);

  if (auto I = identifiers.find(name);
      I != identifiers.end())
    return *I->second;

  // we need to either copy instruction(s) from src, or it's an error
  auto I_src = identifiers_src.find(name);
  if (I_src == identifiers_src.end())
    error("Cannot declare an input variable in the target: " + name);

  auto val_src = I_src->second;
  assert(!dynamic_cast<Input*>(val_src));

  auto instr_src = dynamic_cast<Instr*>(val_src);
  assert(instr_src);
  auto tgt_instr = instr_src->dup("");

  for (auto &op : instr_src->operands()) {
    if (dynamic_cast<Input*>(op)) {
      tgt_instr->rauw(*op, *identifiers.at(op->getName()));
    } else if (dynamic_cast<UndefValue*>(op)) {
      auto newop = make_unique<UndefValue>(op->getType());
      tgt_instr->rauw(*op, *newop.get());
      fn->addUndef(move(newop));
    } else if (dynamic_cast<PoisonValue*>(op)) {
      auto newop = make_unique<PoisonValue>(op->getType());
      tgt_instr->rauw(*op, *newop.get());
      fn->addConstant(move(newop));
    } else if (dynamic_cast<NullPointerValue*>(op)) {
      auto newop = make_unique<NullPointerValue>(op->getType());
      tgt_instr->rauw(*op, *newop.get());
      fn->addConstant(move(newop));
    } else if (auto c = dynamic_cast<IntConst*>(op)) {
      auto newop = make_unique<IntConst>(*c);
      tgt_instr->rauw(*op, *newop.get());
      fn->addConstant(move(newop));
    } else if (auto c = dynamic_cast<FloatConst*>(op)) {
      auto newop = make_unique<FloatConst>(*c);
      tgt_instr->rauw(*op, *newop.get());
      fn->addConstant(move(newop));
    } else if (dynamic_cast<ConstantInput*>(op)) {
      assert(0 && "TODO");
    } else if (dynamic_cast<ConstantBinOp*>(op)) {
      assert(0 && "TODO");
    } else if (dynamic_cast<ConstantFn*>(op)) {
      assert(0 && "TODO");
    } else if (dynamic_cast<Instr*>(op)) {
      // FIXME: support for PHI nodes (cyclic graph)
      tgt_instr->rauw(*op, get_or_copy_instr(op->getName()));
    } else {
      UNREACHABLE();
    }
  }

  auto ret = tgt_instr.get();
  identifiers.emplace(name, ret);
  bb->addInstr(move(tgt_instr));
  return *ret;
}

static Value& parse_aggregate_constant(Type &type, token close_tk) {
  vector<Value*> vals;
  do {
    Type &elemTy = parse_scalar_type();
    Value *elem = &parse_operand(elemTy);
    vals.emplace_back(elem);
  } while (tokenizer.consumeIf(COMMA));

  tokenizer.ensure(close_tk);

  auto c = make_unique<AggregateValue>(type, move(vals));
  auto ret = c.get();
  fn->addConstant(move(c));
  return *ret;
}

static Value& parse_operand(Type &type) {
  switch (auto t = *tokenizer) {
  case NUM:
    return get_constant(yylval.num, type);
  case FP_NUM:
    return get_fp_constant(yylval.fp_num, type);
  case NUM_STR:
    return get_num_constant(yylval.str, type);
  case CSLT:
    return parse_aggregate_constant(type, CSGT);
  case LSQBRACKET:
    return parse_aggregate_constant(type, RSQBRACKET);
  case TRUE:
    return get_constant(1, *int_types[1]);
  case FALSE:
    return get_constant(0, *int_types[1]);
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
  case NULLTOKEN: {
    auto val = make_unique<NullPointerValue>(type);
    auto ret = val.get();
    fn->addConstant(move(val));
    return *ret;
  }
  case REGISTER: {
    string id(yylval.str);
    if (auto I = identifiers.find(id);
        I != identifiers.end())
      return *I->second;

    if (parse_src) {
      auto input = make_unique<Input>(type, string(id));
      auto ret = input.get();
      fn->addInput(move(input));
      identifiers.emplace(move(id), ret);
      return *ret;
    }
    return get_or_copy_instr(id);
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

static unsigned parse_nsw_nuw() {
  unsigned flags = BinOp::None;
  while (true) {
    if (tokenizer.consumeIf(NSW)) {
      flags |= BinOp::NSW;
    } else if (tokenizer.consumeIf(NUW)) {
      flags |= BinOp::NUW;
    } else {
      break;
    }
  }
  return flags;
}

static unsigned parse_exact() {
  if (tokenizer.consumeIf(EXACT))
    return BinOp::Exact;
  return BinOp::None;
}

static FastMathFlags parse_fast_math(token op_token) {
  FastMathFlags fmath;
  while (true) {
    if (tokenizer.consumeIf(NNAN)) {
      fmath.flags |= FastMathFlags::NNaN;
    } else if (tokenizer.consumeIf(NINF)) {
      fmath.flags |= FastMathFlags::NInf;
    } else if (tokenizer.consumeIf(NSZ)) {
      fmath.flags |= FastMathFlags::NSZ;
    } else {
      break;
    }
  }

  switch (op_token) {
  case FADD:
  case FSUB:
  case FMUL:
  case FDIV:
  case FREM:
  case FCMP:
  case FMA:
  case FMAX:
  case FMIN:
    break;
  default:
    if (!fmath.isNone())
      error("Unexpected fast-math tokens");
  }
  return fmath;
}


static unsigned parse_binop_flags(token op_token) {
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
  case FADD:
  case FSUB:
  case FMUL:
  case FDIV:
  case FREM:
  case FMAX:
  case FMIN:
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
  case SADD_OVERFLOW:
  case UADD_OVERFLOW:
  case SSUB_OVERFLOW:
  case USUB_OVERFLOW:
  case SMUL_OVERFLOW:
  case UMUL_OVERFLOW:
  case UMIN:
  case UMAX:
  case SMIN:
  case SMAX:
  case ABS:
    return BinOp::None;
  default:
    UNREACHABLE();
  }
}

static unique_ptr<Instr> parse_binop(string_view name, token op_token) {
  auto flags = parse_binop_flags(op_token);
  auto fmath = parse_fast_math(op_token);
  auto &type = parse_type();
  auto &a = parse_operand(type);
  parse_comma();
  Type &type_rhs = try_parse_type(type);
  auto &b = parse_operand(type_rhs);
  Type *rettype = &type;

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
  case SADD_OVERFLOW:
    op = BinOp::SAdd_Overflow;
    rettype = &get_overflow_type(type);
    break;
  case UADD_OVERFLOW:
    op = BinOp::UAdd_Overflow;
    rettype = &get_overflow_type(type);
    break;
  case SSUB_OVERFLOW:
    op = BinOp::SSub_Overflow;
    rettype = &get_overflow_type(type);
    break;
  case USUB_OVERFLOW:
    op = BinOp::USub_Overflow;
    rettype = &get_overflow_type(type);
    break;
  case SMUL_OVERFLOW:
    op = BinOp::SMul_Overflow;
    rettype = &get_overflow_type(type);
    break;
  case UMUL_OVERFLOW:
    op = BinOp::UMul_Overflow;
    rettype = &get_overflow_type(type);
    break;
  case FADD: op = BinOp::FAdd; break;
  case FSUB: op = BinOp::FSub; break;
  case FMUL: op = BinOp::FMul; break;
  case FDIV: op = BinOp::FDiv; break;
  case FREM: op = BinOp::FRem; break;
  case FMAX: op = BinOp::FMax; break;
  case FMIN: op = BinOp::FMin; break;
  case UMIN: op = BinOp::UMin; break;
  case UMAX: op = BinOp::UMax; break;
  case SMIN: op = BinOp::SMin; break;
  case SMAX: op = BinOp::SMax; break;
  case ABS:
    op = BinOp::Abs;
    break;
  default:
    UNREACHABLE();
  }
  return make_unique<BinOp>(*rettype, string(name), a, b, op, flags, fmath);
}

static unique_ptr<Instr> parse_unaryop(string_view name, token op_token) {
  UnaryOp::Op op;
  switch (op_token) {
  case BITREVERSE: op = UnaryOp::BitReverse; break;
  case BSWAP:      op = UnaryOp::BSwap; break;
  case CTPOP:      op = UnaryOp::Ctpop; break;
  case FNEG:       op = UnaryOp::FNeg; break;
  default:
    UNREACHABLE();
  }

  auto &ty = parse_type();
  auto &a = parse_operand(ty);
  return make_unique<UnaryOp>(ty, string(name), a, op);
}

static unique_ptr<Instr> parse_unary_reduction_op(string_view name,
                                                  token op_token) {
  UnaryReductionOp::Op op;
  switch (op_token) {
  case REDUCE_ADD: op = UnaryReductionOp::Add; break;
  case REDUCE_MUL: op = UnaryReductionOp::Mul; break;
  case REDUCE_AND: op = UnaryReductionOp::And; break;
  case REDUCE_OR:  op = UnaryReductionOp::Or;  break;
  case REDUCE_XOR: op = UnaryReductionOp::Xor; break;
  case REDUCE_SMAX: op = UnaryReductionOp::SMax; break;
  case REDUCE_SMIN: op = UnaryReductionOp::SMin; break;
  case REDUCE_UMAX: op = UnaryReductionOp::UMax; break;
  case REDUCE_UMIN: op = UnaryReductionOp::UMin; break;
  default: UNREACHABLE();
  }

  auto &op_ty = parse_type();
  auto &ty =
      op_ty.isVectorType() ? op_ty.getAsAggregateType()->getChild(0) : op_ty;

  auto &a = parse_operand(op_ty);
  return make_unique<UnaryReductionOp>(ty, string(name), a, op);
}

static unique_ptr<Instr> parse_ternary(string_view name, token op_token) {
  auto fmath = parse_fast_math(op_token);

  TernaryOp::Op op;
  switch (op_token) {
  case FSHL: op = TernaryOp::FShl; break;
  case FSHR: op = TernaryOp::FShr; break;
  case FMA:  op = TernaryOp::FMA; break;
  default:
    UNREACHABLE();
  }

  auto &aty = parse_type();
  auto &a = parse_operand(aty);
  parse_comma();
  auto &bty = parse_type();
  auto &b = parse_operand(bty);
  parse_comma();
  auto &cty = parse_type();
  auto &c = parse_operand(cty);
  return make_unique<TernaryOp>(aty, string(name), a, b, c, op, fmath);
}

static unique_ptr<Instr> parse_conversionop(string_view name, token op_token) {
  // op ty %op to ty2
  auto &opty = parse_type();
  auto &val = parse_operand(opty);
  auto &ty2 = parse_type(/*optional=*/!tokenizer.consumeIf(TO));

  ConversionOp::Op op;
  switch (op_token) {
  case BITCAST:  op = ConversionOp::BitCast; break;
  case SEXT:     op = ConversionOp::SExt; break;
  case ZEXT:     op = ConversionOp::ZExt; break;
  case TRUNC:    op = ConversionOp::Trunc; break;
  case SITOFP:   op = ConversionOp::SIntToFP; break;
  case UITOFP:   op = ConversionOp::UIntToFP; break;
  case FPTOSI:   op = ConversionOp::FPToSInt; break;
  case FPTOUI:   op = ConversionOp::FPToUInt; break;
  case FPEXT:    op = ConversionOp::FPExt; break;
  case FPTRUNC:  op = ConversionOp::FPTrunc; break;
  case PTRTOINT: op = ConversionOp::Ptr2Int; break;
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

static unique_ptr<Instr> parse_extractvalue(string_view name) {
  auto &type = parse_type();
  auto &val = parse_operand(type);
  auto instr = make_unique<ExtractValue>(get_sym_type(), string(name), val);

  while (true) {
    if (!tokenizer.consumeIf(COMMA))
      break;
    instr->addIdx((unsigned)parse_number());
  }

  return instr;
}

static unique_ptr<Instr> parse_insertvalue(string_view name) {
  auto &type = parse_type();
  auto &val = parse_operand(type);
  parse_comma();
  auto &elt_ty = parse_type();
  auto &elt = parse_operand(elt_ty);
  auto instr = make_unique<InsertValue>(type, string(name), val, elt);

  while (true) {
    if (!tokenizer.consumeIf(COMMA))
      break;
    instr->addIdx((unsigned)parse_number());
  }

  return instr;
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

static unique_ptr<Instr> parse_fcmp(string_view name) {
  // fcmp cond ty %a, &b
  FCmp::Cond cond = FCmp::OEQ;
  auto cond_t = *tokenizer;
  switch (cond_t) {
  case OEQ:   cond = FCmp::OEQ; break;
  case OGT:   cond = FCmp::OGT; break;
  case OGE:   cond = FCmp::OGE; break;
  case OLT:   cond = FCmp::OLT; break;
  case OLE:   cond = FCmp::OLE; break;
  case ONE:   cond = FCmp::ONE; break;
  case ORD:   cond = FCmp::ORD; break;
  case UEQ:   cond = FCmp::UEQ; break;
  case UGT:   cond = FCmp::UGT; break;
  case UGE:   cond = FCmp::UGE; break;
  case ULT:   cond = FCmp::ULT; break;
  case ULE:   cond = FCmp::ULE; break;
  case UNE:   cond = FCmp::UNE; break;
  case UNO:   cond = FCmp::UNO; break;
  case TRUE:                    break;
  case FALSE:                   break;
  default:
    error("Expected fcmp cond", cond_t);
  }

  auto &ty = parse_type();
  auto &a = parse_operand(ty);
  parse_comma();
  auto &b = parse_operand(ty);
  auto &bool_ty = *int_types[1].get();

  switch (cond_t) {
  case TRUE:
  case FALSE:
    return make_unique<UnaryOp>(bool_ty, string(name),
                                get_constant(cond_t == TRUE, bool_ty),
                                UnaryOp::Copy);
  default:
    return make_unique<FCmp>(bool_ty, string(name), cond, a, b,
                             parse_fast_math(FCMP));
  }
  UNREACHABLE();
}

static unique_ptr<Instr> parse_freeze(string_view name) {
  // freeze ty %op
  auto &ty = parse_type();
  auto &op = parse_operand(ty);
  return make_unique<Freeze>(ty, string(name), op);
}

static unique_ptr<Instr> parse_free() {
  // free * %op
  auto &ty = parse_type();
  auto &op = parse_operand(ty);
  return make_unique<Free>(op);
}

static unique_ptr<Instr> parse_call(string_view name) {
  // call ty name(ty_1 %op_1, ..., ty_n %op_n)
  auto &ret_ty = parse_type();
  auto fn_name = parse_global_name();
  tokenizer.ensure(LPAREN);

  vector<Value*> args;
  bool first = true;

  while (tokenizer.peek() != RPAREN) {
    if (!first)
      tokenizer.ensure(COMMA);
    first = false;
    auto &ty = parse_type();
    args.emplace_back(&parse_operand(ty));
  }
  tokenizer.ensure(RPAREN);

  FnAttrs attrs;
  while (true) {
    switch (auto t = *tokenizer) {
    case NOREAD:  attrs.set(FnAttrs::NoRead); break;
    case NOWRITE: attrs.set(FnAttrs::NoWrite); break;
    default:
      tokenizer.unget(t);
      goto exit;
    }
  }
exit:
  auto call = make_unique<FnCall>(ret_ty, string(name), move(fn_name),
                                  move(attrs));

  for (auto arg : args) {
    call->addArg(*arg, ParamAttrs::None);
  }
  return call;
}

static unique_ptr<Instr> parse_malloc(string_view name) {
  // %p = malloc ty %sz
  auto &ty = parse_type();
  auto &op = parse_operand(ty);
  // Malloc returns a pointer at address space 0
  Type &pointer_type = get_pointer_type(0);
  return make_unique<Malloc>(pointer_type, string(name), op, false);
}

static unique_ptr<Instr> parse_extractelement(string_view name) {
  // %p = extractelement vty %v, ty %idx
  auto &ty_a = parse_type();
  auto &a = parse_operand(ty_a);
  parse_comma();
  auto &ty_idx = parse_type();
  auto &idx = parse_operand(ty_idx);
  return make_unique<ExtractElement>(get_sym_type(), string(name), a, idx);
}

static unique_ptr<Instr> parse_insertelement(string_view name) {
  // %p = insertelement vty %v, ty %n, ty %idx
  auto &ty_a = parse_type();
  auto &a = parse_operand(ty_a);
  parse_comma();
  auto &ty_e = parse_type();
  auto &e = parse_operand(ty_e);
  parse_comma();
  auto &ty_idx = parse_type();
  auto &idx = parse_operand(ty_idx);
  return make_unique<InsertElement>(get_sym_type(), string(name), a, e, idx);
}

static unique_ptr<Instr> parse_shufflevector(string_view name) {
  // %p = shufflevector ty %a, ty %b, ty %c
  auto &ty_a = parse_type();
  auto &a = parse_operand(ty_a);
  parse_comma();
  auto &ty_b = parse_type();
  auto &b = parse_operand(ty_b);

  vector<unsigned> mask;
  while (tokenizer.consumeIf(COMMA)) {
    mask.push_back((unsigned)parse_number());
  }
  return make_unique<ShuffleVector>(get_sym_type(), string(name), a, b,
                                    move(mask));
}

static unique_ptr<Instr> parse_copyop(string_view name, token t) {
  tokenizer.unget(t);
  auto &ty = parse_type();
  auto &op = parse_operand(ty);
  return make_unique<UnaryOp>(ty, string(name), op, UnaryOp::Copy);
}

static unique_ptr<Instr> parse_instr(string_view name) {
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
  case SADD_OVERFLOW:
  case UADD_OVERFLOW:
  case SSUB_OVERFLOW:
  case USUB_OVERFLOW:
  case SMUL_OVERFLOW:
  case UMUL_OVERFLOW:
  case FADD:
  case FSUB:
  case FMUL:
  case FDIV:
  case FREM:
  case FMAX:
  case FMIN:
  case UMIN:
  case UMAX:
  case SMIN:
  case SMAX:
  case ABS:
    return parse_binop(name, t);
  case BITREVERSE:
  case BSWAP:
  case CTPOP:
  case FNEG:
    return parse_unaryop(name, t);
  case REDUCE_ADD:
  case REDUCE_MUL:
  case REDUCE_AND:
  case REDUCE_OR:
  case REDUCE_XOR:
  case REDUCE_SMAX:
  case REDUCE_SMIN:
  case REDUCE_UMAX:
  case REDUCE_UMIN:
    return parse_unary_reduction_op(name, t);
  case FSHL:
  case FSHR:
  case FMA:
    return parse_ternary(name, t);
  case BITCAST:
  case SEXT:
  case ZEXT:
  case TRUNC:
  case SITOFP:
  case UITOFP:
  case FPTOSI:
  case FPTOUI:
  case FPEXT:
  case FPTRUNC:
  case PTRTOINT:
    return parse_conversionop(name, t);
  case SELECT:
    return parse_select(name);
  case EXTRACTVALUE:
    return parse_extractvalue(name);
  case INSERTVALUE:
    return parse_insertvalue(name);
  case ICMP:
    return parse_icmp(name);
  case FCMP:
    return parse_fcmp(name);
  case FREE:
    return parse_free();
  case FREEZE:
    return parse_freeze(name);
  case CALL:
    return parse_call(name);
  case MALLOC:
    return parse_malloc(name);
  case EXTRACTELEMENT:
    return parse_extractelement(name);
  case INSERTELEMENT:
    return parse_insertelement(name);
  case SHUFFLEVECTOR:
    return parse_shufflevector(name);
  case INT_TYPE:
  case HALF:
  case FLOAT:
  case DOUBLE:
  case VECTOR_TYPE_PREFIX:
  case NUM:
  case FP_NUM:
  case TRUE:
  case FALSE:
  case UNDEF:
  case POISON:
  case REGISTER:
  case ARRAY_TYPE_PREFIX:
    return parse_copyop(name, t);
  default:
    tokenizer.unget(t);
    return nullptr;
  }
  UNREACHABLE();
}

static unique_ptr<Instr> parse_assume(bool if_non_poison) {
  tokenizer.ensure(LPAREN);
  auto &val = parse_operand(*int_types[1].get());
  tokenizer.ensure(RPAREN);
  return make_unique<Assume>(val, if_non_poison);
}

static unique_ptr<Instr> parse_return() {
  auto &type = parse_type();
  auto &val = parse_operand(type);
  return make_unique<Return>(type, val);
}

static void parse_fn(Function &f) {
  fn = &f;
  bb = &f.getBB("");
  bool has_return = false;

  while (true) {
    switch (auto t = *tokenizer) {
    case ASSUME:
      bb->addInstr(parse_assume(false));
      break;
    case ASSUME_NON_POISON:
      bb->addInstr(parse_assume(true));
      break;
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
      string_view name;
      if (t == REGISTER) {
        name = yylval.str;
        tokenizer.ensure(EQUALS);
      } else
        tokenizer.unget(t);

      auto i = parse_instr(name);
      if (!i) {
        if (name.empty())
          goto exit;
        error("Instruction expected", *tokenizer);
      }

      if (!name.empty()) {
        if (!identifiers.emplace(name, i.get()).second)
          error("Duplicated assignment to " + string(name));
      }
      bb->addInstr(move(i));
      break;
    }
  }

exit:
  if (!has_return) {
    if (bb->empty())
      error("Block cannot be empty");

    auto &val = bb->back();
    bb->addInstr(make_unique<Return>(val.getType(), val));
    f.setType(val.getType());
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
    sym_num = struct_num = 0;
    parse_src = true;

    parse_name(t);
    parse_pre(t);
    parse_fn(t.src);
    parse_arrow();

    // copy inputs from src to target
    decltype(identifiers) identifiers_tgt;
    for (auto &val : t.src.getInputs()) {
      auto &name = val.getName();
      if (dynamic_cast<const Input*>(&val)) {
        auto input = make_unique<Input>(val.getType(), string(name));
        identifiers_tgt.emplace(name, input.get());
        t.tgt.addInput(move(input));
      } else {
        assert(dynamic_cast<const ConstantInput*>(&val));
        auto input = make_unique<ConstantInput>(val.getType(), string(name));
        identifiers_tgt.emplace(name, input.get());
        t.tgt.addInput(move(input));
      }
    }
    identifiers_src = move(identifiers);
    identifiers = move(identifiers_tgt);

    parse_src = false;
    parse_fn(t.tgt);

    // copy any missing instruction in tgt from src
    for (auto &[name, val] : identifiers_src) {
      (void)val;
      get_or_copy_instr(name);
    }

    identifiers.clear();
    identifiers_src.clear();
  }

  return ret;
}


parser_initializer::parser_initializer() {
  int_types.resize(65);
  int_types[1] = make_unique<IntType>("i1", 1);
}

parser_initializer::~parser_initializer() {
  int_types.clear();
  sym_types.clear();
  overflow_aggregate_types.clear();
}

}
