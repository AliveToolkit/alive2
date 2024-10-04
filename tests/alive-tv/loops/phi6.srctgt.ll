; TEST-ARGS: -src-unroll=3 -tgt-unroll=3

declare i32 @f()

define i32 @src(i1 %cond, i1 %cond2) {
entry:
  br label %header
header:
  %a = call i32 @f()
  br i1 %cond, label %loop, label %return
loop:
  br i1 %cond2, label %header, label %return
return:
  %p = phi i32 [0, %header], [%a, %loop]
  ret i32 %p
}

define i32 @tgt(i1 %cond, i1 %cond2) {
entry:
  %fr = freeze i1 %cond2
  %brmerge.demorgan = and i1 %cond, %fr
  br label %header

header:
  %a = call i32 @f()
  br i1 %brmerge.demorgan, label %header, label %return

return:
  %.mux = select i1 %cond, i32 %a, i32 0
  ret i32 %.mux
}
