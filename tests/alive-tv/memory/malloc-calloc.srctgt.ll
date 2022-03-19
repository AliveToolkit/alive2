target triple = "x86-unknown-linux-gnu"
target datalayout = "e-p:32:32"

define i8* @src(i32 %size) {
  %call1 = call i8* @malloc(i32 %size)
  ret i8* %call1
}

define i8* @tgt(i32 %size) {
  %calloc = call i8* @calloc(i32 1, i32 %size)
  ret i8* %calloc
}

declare i8* @calloc(i32, i32)
declare i8* @malloc(i32)
