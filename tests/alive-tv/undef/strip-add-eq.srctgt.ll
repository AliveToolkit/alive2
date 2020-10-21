; TEST-ARGS: -se-verbose
define i32 @src(i32 %x, i32 %y) {
  %v = add i32 %x, 1
  %c = icmp eq i32 %x, %y
  br i1 %c, label %A, label %B
A:
  %w = add i32 %x, 2
  ret i32 %w
B:
  %w2 = add i32 %y, 2
  ret i32 %w2
}

define i32 @tgt(i32 %x, i32 %y) {
  %v = add i32 %x, 1
  %c = icmp eq i32 %x, %y
  br i1 %c, label %A, label %B
A:
  %w = add i32 %x, 2
  ret i32 %w
B:
  %w2 = add i32 %y, 2
  ret i32 %w2
}

; CHECK: (ite (= %x %y) (bvadd #x00000002 %x) (bvadd #x00000002 %y)) / true
