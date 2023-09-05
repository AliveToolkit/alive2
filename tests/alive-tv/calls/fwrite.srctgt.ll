%FILE = type {}
@empty = constant [0 x i8] zeroinitializer
declare i64 @fwrite(ptr, i64, i64, ptr)

define void @src(ptr %fp) {
  %str = getelementptr inbounds [0 x i8], ptr @empty, i64 0, i64 0
  %1 = and i64 2122369476, 2066466721
  %2 = call i64 @fwrite(ptr %str, i64 %1, i64 0, ptr %fp)
  ret void
}

define void @tgt(ptr %fp) {
  ret void
}
