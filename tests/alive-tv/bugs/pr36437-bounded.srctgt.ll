; TEST-ARGS: -src-unroll=2 -tgt-unroll=2
; https://bugs.llvm.org/show_bug.cgi?id=36437
; Bound the # of iterations to 2, because the problematic execution
; happens at the second iteration.

define i32 @src() {
entry:
  store i32 3, i32* @a, align 4
  br label %for.body

for.body:                                         ; preds = %entry, %for.body
  %d.05 = phi i32 [ 0, %entry ], [ -1, %for.body ]
  %0 = load i32*, i32** @c, align 8
  %1 = load i32, i32* %0, align 4
  %and = and i32 %1, %d.05
  store i32 %and, i32* %0, align 4
  %2 = load i32, i32* @a, align 4
  %sext = shl i32 %2, 16
  %conv3 = ashr exact i32 %sext, 16
  %sub = add nsw i32 %conv3, -1
  store i32 %sub, i32* @a, align 4
  %cmp = icmp eq i32 %sub, 1
  br i1 %cmp, label %for.end, label %for.body

for.end:                                          ; preds = %for.body
  %3 = load i32, i32* @b, align 4
  %call = tail call i32 (i8*, ...) @print(i8* nonnull dereferenceable(1) getelementptr inbounds ([15 x i8], [15 x i8]* @.str, i64 0, i64 0), i32 %3)
  ret i32 0
}

define i32 @tgt() {
entry:
  store i32 3, i32* @a, align 4
  br label %for.body

for.body:                                         ; preds = %entry, %for.body
  %phiofops = phi i32 [ 0, %entry ], [ %1, %for.body ]
  %d.05 = phi i32 [ 0, %entry ], [ -1, %for.body ]
  %0 = load i32*, i32** @c, align 8
  %1 = load i32, i32* %0, align 4
  store i32 %phiofops, i32* %0, align 4
  %2 = load i32, i32* @a, align 4
  %sext = shl i32 %2, 16
  %conv3 = ashr exact i32 %sext, 16
  %sub = add nsw i32 %conv3, -1
  store i32 %sub, i32* @a, align 4
  %cmp = icmp eq i32 %sub, 1
  br i1 %cmp, label %for.end, label %for.body

for.end:                                          ; preds = %for.body
  %3 = load i32, i32* @b, align 4
  %call = tail call i32 (i8*, ...) @print(i8* nonnull dereferenceable(1) getelementptr inbounds ([15 x i8], [15 x i8]* @.str, i64 0, i64 0), i32 %3)
  ret i32 0
}

; ERROR: Source is more defined than target

@b = global i32 2, align 4
@c = global i32* @b, align 8
@a = common global i32 0, align 4
@.str = private unnamed_addr constant [15 x i8] c"checksum = %X\0A\00", align 1
declare i32 @print(i8*, ...)
