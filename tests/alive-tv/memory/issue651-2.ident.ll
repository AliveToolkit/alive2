; POR should consider poison-ness

declare void @g()
@p = global i64 0

define void @f(i1 noundef %cond, ptr noundef %ptr) {
entry:
  call void @g()
  store i64 undef, ptr @p, align 8
  br i1 %cond, label %cleanup, label %if.end

if.end:
  load ptr, ptr %ptr, align 8
  br label %cleanup

cleanup:
  ret void
}
