@glb = global {i8, i32} undef, align 4

define i8 @src() {
  %p = bitcast {i8, i32}* @glb to i32*
  store i32 0, i32* %p, align 4 ; fill padding with 0

  %v = load {i8, i32}, {i8, i32}* @glb ;padding should be poison
  store {i8, i32} %v, {i8, i32}* @glb, align 4

  %p2 = bitcast {i8, i32}* @glb to i8*
  %p3 = getelementptr i8, i8* %p2, i64 1
  %padding = load i8, i8* %p3
  ret i8 %padding
}

define i8 @tgt() {
  %p = bitcast {i8, i32}* @glb to i32*
  store i32 0, i32* %p, align 4

  ret i8 undef
}
