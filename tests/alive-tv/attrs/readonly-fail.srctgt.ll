; ERROR: Source is more defined than target

define i8 @src(i8* %p) readonly {
  %a = alloca i8
  store i8 0, i8* %a
  ret i8 2
}

define i8 @tgt(i8* %p) readonly {
  %a = alloca i8
  unreachable
}
