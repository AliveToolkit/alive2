target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i8:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define void @src(i8* %db, i8* %p) {
entry:
  %0 = ptrtoint i8* %p to i8
  %1 = getelementptr i8, i8* %db, i32 0
  %2 = load i8, i8* %1, align 8 ; It was loaded, so %db cannot be null
  %call6 = call i8* @g(i8* %db)
  ret void
}

define void @tgt(i8* %db, i8* %p) {
entry:
  %0 = ptrtoint i8* %p to i8
  %1 = getelementptr i8, i8* %db, i32 0
  %2 = load i8, i8* %1, align 8
  %call6 = call i8* @g(i8* nonnull %db)
  ret void
}

declare i8* @g(i8*)
