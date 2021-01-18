; align at callee doesn't guarantee that the pointer is aligned at caller
define void @src(i8* %p) {
  call void @g(i8* %p)
  load i8, i8* %p
  ret void
}

define void @tgt(i8* %p) {
  call void @g(i8* %p)
  load i8, i8* %p, align 4
  ret void
}

declare void @g(i8* align(4))

; ERROR: Source is more defined than target
