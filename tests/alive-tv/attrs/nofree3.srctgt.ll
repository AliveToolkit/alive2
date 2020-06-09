define void @src() nofree {
  call void @fn()
  ret void
}

define void @tgt() nofree {
  unreachable
}

; ERROR: Source is more defined than target

declare void @fn() nofree
