define void @src(ptr %p) {
    call void @f(ptr %p)
    ret void
}

define void @tgt(ptr %p) {
    call void @f(ptr dereferenceable(2) %p)
    ret void
}

declare void @f(ptr byval(i8))

; ERROR: Source is more defined than target
