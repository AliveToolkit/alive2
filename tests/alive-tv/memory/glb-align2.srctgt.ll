%struct.d = type { i32*, i32* }

@f = external global %struct.d*, align 8

define void @src() {
entry:
  %0 = load %struct.d*, %struct.d** @f, align 8
  %c = getelementptr inbounds %struct.d, %struct.d* %0, i32 0, i32 1
  call void @e(i32** %c)
  inttoptr i64 42 to i8*
  ret void
}

define void @tgt() {
entry:
  %0 = load %struct.d*, %struct.d** @f, align 8
  %c = getelementptr inbounds %struct.d, %struct.d* %0, i32 0, i32 1
  call void @e(i32** nonnull %c)
  ret void
}

declare void @e(i32**)
