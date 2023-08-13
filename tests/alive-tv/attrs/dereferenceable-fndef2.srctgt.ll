define dereferenceable(4) ptr @src() {
  %p = call ptr @f()
  ret ptr %p
}

define dereferenceable(4) ptr @tgt() {
  unreachable
}

; ERROR: Source is more defined than target

declare dereferenceable(4) ptr @f()
