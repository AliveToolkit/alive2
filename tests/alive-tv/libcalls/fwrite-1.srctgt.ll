target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [5 x i8] c"xpto\00", align 1

define i64 @src(ptr %f) {
  %call = call i64 @fwrite(ptr getelementptr inbounds ([5 x i8], ptr @.str, i64 0, i64 0), i64 1, i64 0, ptr %f)
  ret i64 %call
}

define i64 @tgt(ptr %f) {
  ret i64 0
}

declare i64 @fwrite(ptr, i64, i64, ptr)
