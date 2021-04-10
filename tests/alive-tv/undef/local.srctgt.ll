target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define void @src() {
entry:
  %retval = alloca i32, align 4
  call i32 @b(i32* undef)
  ret void
}

define void @tgt() {
entry:
  call i32 @b(i32* undef)
  ret void
}

declare i32* @a()

declare i32 @b(i32*)
