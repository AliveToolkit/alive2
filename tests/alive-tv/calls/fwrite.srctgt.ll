%FILE = type {}
@empty = constant [0 x i8] zeroinitializer
declare i64 @fwrite(i8*, i64, i64, %FILE*)

define void @src(%FILE* %fp) {
  %str = getelementptr inbounds [0 x i8], [0 x i8]* @empty, i64 0, i64 0
  %1 = and i64 2122369476, 2066466721
  %2 = call i64 @fwrite(i8* %str, i64 %1, i64 0, %FILE* %fp)
  ret void
}

define void @tgt(%FILE* %fp) {
  ret void
}
