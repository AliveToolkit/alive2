target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define i32 @src(i32 %c) readnone {
entry:
  switch i32 %c, label %default [
    i32 0, label %sw.bb0
    i32 1, label %sw.bb1
    i32 2, label %sw.bb2
    i32 3, label %sw.bb3
  ]

sw.bb0:
  br label %return

sw.bb1:
  br label %return

sw.bb2:
  br label %return

sw.bb3:
  br label %return

default:
  br label %return

return:
  %retval.0 = phi i32 [ 33, %sw.bb0 ], [ 2, %sw.bb1 ], [ 5, %sw.bb2 ], [ 1, %sw.bb3 ], [ 0, %default ]
  ret i32 %retval.0
}


@switch.table = hidden unnamed_addr constant [4 x i32] [i32 33, i32 2, i32 5, i32 1], align 4

define i32 @tgt(i32 %c) readnone {
entry:
  %0 = icmp ule i32 %c, 3
  br i1 %0, label %switch.lookup, label %default

switch.lookup:
  %switch.gep = getelementptr inbounds [4 x i32], [4 x i32]* @switch.table, i64 0, i32 %c
  %switch.load = load i32, i32* %switch.gep, align 4
  ret i32 %switch.load

default:
  ret i32 0
}

