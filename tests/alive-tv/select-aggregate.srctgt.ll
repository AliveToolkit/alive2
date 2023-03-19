; InstCombine/cast.ll , PR28745()
target datalayout="E-p:64:64:64-p1:32:32:32-p2:64:64:64-p3:64:64:64-a0:0:8-f32:32:32-f64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-v64:64:64-v128:128:128-n8:16:32:64"

define i64 @src() {
  %elem = extractelement <2 x i16> bitcast (<1 x i32> <i32 1> to <2 x i16>), i32 0
  %cmp = icmp eq i16 %elem, 0
  %v = select i1 %cmp, { i32 } { i32 1 }, { i32 } zeroinitializer
  %ev = extractvalue { i32 } %v, 0
  %b = zext i32 %ev to i64
  ret i64 %b
}

define i64 @tgt() {
  ret i64 1
}
