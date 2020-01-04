; TEST-ARGS: -smt-to=10000

define i16 @f1(half %x) {
  %a = alloca half
  store half %x, half* %a
  %p = bitcast half* %a to i16*
  %i = load i16, i16* %p
  ret i16 %i
}
