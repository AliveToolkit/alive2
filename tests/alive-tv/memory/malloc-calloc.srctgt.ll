target triple = "x86-unknown-linux-gnu"
target datalayout = "e-p:32:32"

define ptr @src(i32 %size) {
  %call1 = call ptr @malloc(i32 %size)
  ret ptr %call1
}

define ptr @tgt(i32 %size) {
  %calloc = call ptr @calloc(i32 1, i32 %size)
  ret ptr %calloc
}

declare ptr @calloc(i32, i32)
declare ptr @malloc(i32)
