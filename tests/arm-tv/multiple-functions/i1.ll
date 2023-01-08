; # # # # . . . . . . . . . . . . . . .    +
; # # # # . . . . . . . . . . . . . . .    -
; # # # # . . . . . . . . . . . . . . .    ^
; # # # # . . . . . . . . . . . . . . .    !=
; . . . . # . . . . . . . . . . . . . .    |
; . . . . . # . . . . . . . . . . . . .    ==
; . . . . . . # # . . . . . . . . . . .    &
; . . . . . . # # . . . . . . . . . . .    *
; . . . . . . . . # # . . . . . . . . .    >=u
; . . . . . . . . # # . . . . . . . . .    <=s
; . . . . . . . . . . # # . . . . . . .    <=u
; . . . . . . . . . . # # . . . . . . .    >=s
; . . . . . . . . . . . . # # . . . . .    >s
; . . . . . . . . . . . . # # . . . . .    <u
; . . . . . . . . . . . . . . # # # # .    >u
; . . . . . . . . . . . . . . # # # # .    <s
; . . . . . . . . . . . . . . # # # # .    <<
; . . . . . . . . . . . . . . # # # # .    >>l
; . . . . . . . . . . . . . . . . . . #    >>a

define i1 @add(i1, i1) {
  %r = add i1 %0, %1
  ret i1 %r
}

define i1 @sub(i1, i1) {
  %r = sub i1 %0, %1
  ret i1 %r
}

define i1 @xor(i1, i1) {
  %r = xor i1 %0, %1
  ret i1 %r
}

define i1 @ne(i1, i1) {
  %r = icmp ne i1 %0, %1
  ret i1 %r
}

define i1 @or(i1, i1) {
  %r = or i1 %0, %1
  ret i1 %r
}

define i1 @eq(i1, i1) {
  %r = icmp eq i1 %0, %1
  ret i1 %r
}

define i1 @and(i1, i1) {
  %r = and i1 %0, %1
  ret i1 %r
}

define i1 @mul(i1, i1) {
  %r = mul i1 %0, %1
  ret i1 %r
}

define i1 @uge(i1, i1) {
  %r = icmp uge i1 %0, %1
  ret i1 %r
}

define i1 @sle(i1, i1) {
  %r = icmp sle i1 %0, %1
  ret i1 %r
}

define i1 @ule(i1, i1) {
  %r = icmp ule i1 %0, %1
  ret i1 %r
}

define i1 @sge(i1, i1) {
  %r = icmp sge i1 %0, %1
  ret i1 %r
}

define i1 @sgt(i1, i1) {
  %r = icmp sgt i1 %0, %1
  ret i1 %r
}

define i1 @ult(i1, i1) {
  %r = icmp ult i1 %0, %1
  ret i1 %r
}

define i1 @ugt(i1, i1) {
  %r = icmp ugt i1 %0, %1
  ret i1 %r
}

define i1 @slt(i1, i1) {
  %r = icmp slt i1 %0, %1
  ret i1 %r
}

define i1 @shl(i1, i1) {
  %r = shl i1 %0, %1
  ret i1 %r
}

define i1 @lshr(i1, i1) {
  %r = lshr i1 %0, %1
  ret i1 %r
}

define i1 @ashr(i1, i1) {
  %r = ashr i1 %0, %1
  ret i1 %r
}

define i1 @sdiv(i1, i1) {
  %r = sdiv i1 %0, %1
  ret i1 %r
}

define i1 @udiv(i1, i1) {
  %r = udiv i1 %0, %1
  ret i1 %r
}

define i1 @srem(i1, i1) {
  %r = srem i1 %0, %1
  ret i1 %r
}

define i1 @urem(i1, i1) {
  %r = urem i1 %0, %1
  ret i1 %r
}

declare i1 @llvm.sadd.sat.i1(i1, i1)

define i1 @sadd_sat(i1, i1) {
  %r = call i1 @llvm.sadd.sat.i1(i1 %0, i1 %1)
  ret i1 %r
}

declare i1 @llvm.ssub.sat.i1(i1, i1)

define i1 @ssub_sat(i1, i1) {
  %r = call i1 @llvm.ssub.sat.i1(i1 %0, i1 %1)
  ret i1 %r
}

declare i1 @llvm.uadd.sat.i1(i1, i1)

define i1 @uadd_sat(i1, i1) {
  %r = call i1 @llvm.uadd.sat.i1(i1 %0, i1 %1)
  ret i1 %r
}

declare i1 @llvm.usub.sat.i1(i1, i1)

define i1 @usub_sat(i1, i1) {
  %r = call i1 @llvm.usub.sat.i1(i1 %0, i1 %1)
  ret i1 %r
}

