declare void @f(i1)

; show that neither a or b is undef
define void @src(i1 %a, i1 %b) {
  %c = add i1 %a, %b
  call void @f(i1 noundef %c)
  ret void
}

define void @tgt(i1 %a, i1 %b) {
  %c = add i1 %a, %b
  call void @f(i1 noundef %c)
  br i1 %a, label %A, label %B
A:
  br i1 %b, label %B, label %C
B:
  ret void
C:
  ret void
}
