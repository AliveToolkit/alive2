target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@x = global i8* null
declare void @f(i8*)

define i32 @src() {
  %e = alloca i32, align 4
  store i32 1, i32* %e
  %p = load i8*, i8** @x, align 8
  call void @f(i8* %p) writeonly argmemonly
  %v = load i32, i32* %e
  ret i32 %v
}

define i32 @tgt() {
  %p = load i8*, i8** @x, align 8
  call void @f(i8* %p) writeonly argmemonly
  ret i32 1
}
