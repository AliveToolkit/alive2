; POR should consider poison-ness

declare void @g()
@p = global i64 0

define void @f(i1 noundef %cond, i8** noundef %ptr) {
entry:
  call void @g()
  store i64 undef, i64* @p, align 8
  br i1 %cond, label %cleanup, label %if.end

if.end:
  load i8*, i8** %ptr, align 8
  br label %cleanup

cleanup:
  ret void
}
