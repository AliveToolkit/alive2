; https://bugs.llvm.org/show_bug.cgi?id=27575
; To find counter-example from this, function should be able to throw or exit
define void @src() {
  %p = alloca i32, align 4
  %a = load i32, i32* %p, align 4
  call void @foo(i32 %a)
  store i32 sdiv (i32 1, i32 sub (i32 ptrtoint (i32* @G to i32), i32 ptrtoint (i32* @G to i32))), i32* %p, align 4
  ret void
}

@G = external global i32, align 4

define void @tgt() {
  call void @foo(i32 sdiv (i32 1, i32 sub (i32 ptrtoint (i32* @G to i32), i32 ptrtoint (i32* @G to i32))))
  ret void
}

declare void @foo(i32)

; XFAIL
