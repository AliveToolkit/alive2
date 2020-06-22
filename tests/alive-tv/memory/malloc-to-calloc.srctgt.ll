target datalayout = "e-p:32:32:32"

declare i8* @memset(i8*, i32, i32)
declare void @llvm.memset.p0i8.i32(i8* nocapture writeonly, i8, i32, i32, i1)
declare i8* @malloc(i32)
declare i8* @calloc(i32, i32)

define i8* @src(i32 %size) {
  %call1 = call i8* @malloc(i32 %size)
  %call2 = call i8* @memset(i8* %call1, i32 0, i32 %size)
  ret i8* %call2
}

define i8* @tgt(i32 %size) {
  %calloc = call i8* @calloc(i32 1, i32 %size)
  ret i8* %calloc
}
