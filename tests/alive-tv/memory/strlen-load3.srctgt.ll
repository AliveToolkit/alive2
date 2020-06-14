target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i64 @src(i8* %p) {
  %v = load i8, i8* %p
  %isnull = icmp eq i8 %v, 0
  br i1 %isnull, label %A, label %B
A:
  ret i64 0
B:
  %p2 = getelementptr inbounds i8, i8* %p, i64 1
  %l = call i64 @strlen(i8* %p2)
  %l2 = add i64 %l, 1
  ret i64 %l2
}

define i64 @tgt(i8* %p) {
  %l = call i64 @strlen(i8* %p)
  ret i64 %l
}

declare i64 @strlen(i8*)
