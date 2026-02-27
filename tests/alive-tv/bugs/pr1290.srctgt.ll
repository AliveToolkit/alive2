define void @f(ptr %out1) {
entry:
  store i32 0, ptr %out1, align 4
  %arrayidx1 = getelementptr i8, ptr %out1, i64 4
  store i32 0, ptr %arrayidx1, align 4
  %arrayidx2 = getelementptr i8, ptr %out1, i64 8
  store i32 0, ptr %arrayidx2, align 4
  %arrayidx3 = getelementptr i8, ptr %out1, i64 12
  store i32 0, ptr %arrayidx3, align 4
  %arrayidx4 = getelementptr i8, ptr %out1, i64 16
  store i32 0, ptr %arrayidx4, align 4
  %arrayidx5 = getelementptr i8, ptr %out1, i64 20
  store i32 0, ptr %arrayidx5, align 4
  %arrayidx6 = getelementptr i8, ptr %out1, i64 24
  store i32 0, ptr %arrayidx6, align 4
  %arrayidx7 = getelementptr i8, ptr %out1, i64 28
  store i32 0, ptr %arrayidx7, align 4
  %arrayidx8 = getelementptr i8, ptr %out1, i64 32
  store i32 0, ptr %arrayidx8, align 4
  ret void
}
