declare i1 @use(i32)

define void @src() {
 entry:
  %t = alloca i32
  %v = load i32, i32* %t
  %c = call i1 @use(i32 %v)
  store i32 46, i32* %t
  store i32 42, i32* %t
  %v2 = load i32, i32* %t
  %c2 = call i1 @use(i32 %v2)
  store i32 46, i32* %t
  store i32 42, i32* %t
  ret void
}

define void @tgt() {
entry:
  %c = tail call i1 @use(i32 undef)
  %c2 = tail call i1 @use(i32 undef)
  ret void
}

; ERROR: Source is more defined than target
