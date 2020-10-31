target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare void @g()

define void @src(i1 %cmp) {
entry:
  %call = call i32* @__errno_location()
  call void @g()
  br i1 %cmp, label %if.then, label %if.end

if.then:
  %call2 = call i32* @__errno_location()
  store i32 0, i32* %call2, align 4
  br label %if.end

if.end:
  ret void
}

define void @tgt(i1 %cmp) {
entry:
  %call = call i32* @__errno_location()
  call void @g()
  br i1 %cmp, label %if.then, label %if.else

if.then:
  store i32 0, i32* %call, align 4
  br label %if.end

if.else:
  br label %if.end

if.end:
  ret void
}

declare i32* @__errno_location() readnone
