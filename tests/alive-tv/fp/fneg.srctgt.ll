define void @src(half %x, half %y) {
  %s = fsub half -0.0, %x
  call void @fn(half %s)
  ret void
}

define void @tgt(half %x, half %y) {
  %s = fneg half %x
  call void @fn(half %s)
  ret void
}

declare void @fn(half)
