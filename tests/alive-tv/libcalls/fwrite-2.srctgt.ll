target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [5 x i8] c"xpto\00", align 1

define void @src(ptr %f) {
  %call = call i64 @fwrite(ptr getelementptr inbounds ([5 x i8], ptr @.str, i64 0, i64 0), i64 1, i64 1, ptr %f)
  ret void
}

define void @tgt(ptr %f) {
  %call = call i32 @fputc(i32 120, ptr %f)
  ret void
}

declare i64 @fwrite(ptr, i64, i64, ptr)
declare i32 @fputc(i32, ptr)
