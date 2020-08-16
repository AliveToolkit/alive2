@g = global {{}, i32} undef

; Does not touch padding

define i32 @src({{}, i32} noundef %cond) {
  ; A 4-byte store; https://godbolt.org/z/c4qfGP
  store {{}, i32} %cond, {{}, i32}* @g
  %p = bitcast {{}, i32}* @g to i8*
  %v = load i8, i8* %p
  %v.fr = freeze i8 %v
  %c = icmp eq i8 %v.fr, 0
  br i1 %c, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}

define i32 @tgt({{}, i32} noundef %cond) {
  store {{}, i32} %cond, {{}, i32}* @g
  %p = bitcast {{}, i32}* @g to i8*
  %v = load i8, i8* %p
  %c = icmp eq i8 %v, 0
  br i1 %c, label %A, label %B
A:
  ret i32 0
B:
  ret i32 1
}
