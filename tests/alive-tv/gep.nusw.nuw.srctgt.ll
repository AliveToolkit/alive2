; TEST_ARGS: -disable-undef-input

target datalayout = "p:8:8"

define ptr @src1(ptr %x, i8 %i) {
  %p = getelementptr inbounds i8, ptr %x, i8 %i
  ret ptr %p
}

; Can refine to inbounds to nusw.
define ptr @tgt1(ptr %x, i8 %i) {
  %p = getelementptr nusw i8, ptr %x, i8 %i
  ret ptr %p
}

define ptr @src2(ptr %x, i16 %i) {
  %p = getelementptr nuw i8, ptr %x, i16 %i
  ret ptr %p
}

; gep nuw implies trunc nuw.
define ptr @tgt2(ptr %x, i16 %i) {
  %i.trunc = trunc nuw i16 %i to i8
  %p = getelementptr nuw i8, ptr %x, i8 %i.trunc
  ret ptr %p
}

define ptr @src3(ptr %x, i8 %i) {
  %p = getelementptr nuw i16, ptr %x, i8 %i
  ret ptr %p
}

; gep nuw implies mul nuw.
define ptr @tgt3(ptr %x, i8 %i) {
  %i.mul = mul nuw i8 %i, 2
  %p = getelementptr nuw i8, ptr %x, i8 %i.mul
  ret ptr %p
}

define ptr @src4(ptr %x, i8 %i, i8 %j) {
  %p = getelementptr nuw [1 x i8], ptr %x, i8 %i, i8 %j
  ret ptr %p
}

; gep nuw implies add nuw.
define ptr @tgt4(ptr %x, i8 %i, i8 %j) {
  %i.j = add nuw i8 %i, %j
  %p = getelementptr nuw i8, ptr %x, i8 %i.j
  ret ptr %p
}

define i1 @src5(ptr %x, i8 %i) {
  %p = getelementptr nuw i8, ptr %x, i8 %i
  %c = icmp uge ptr %p, %x
  ret i1 %c
}

; gep nuw implies uge.
define i1 @tgt5(ptr %x, i8 %i) {
  ret i1 true
}
