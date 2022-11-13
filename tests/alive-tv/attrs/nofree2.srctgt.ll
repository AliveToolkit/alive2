; ERROR: Source is more defined than target

define void @src() nofree {
  call void @fn()
  ret void
}

define void @tgt() nofree {
  unreachable
}

declare void @fn()
