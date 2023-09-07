; ERROR: Source is more defined than target

@arr = external global [0 x i8]

define i8 @src() {
  %p = getelementptr inbounds i8, ptr @arr, i64 4
  %r = load i8, ptr %p
  ret i8 %r
}

define i8 @tgt() {
  unreachable
}
