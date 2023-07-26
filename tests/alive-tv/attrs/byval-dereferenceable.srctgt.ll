define void @src(ptr %p) {
    call void @f(ptr %p)
    ret void
}

define void @tgt(ptr %p) {
    call void @f(ptr dereferenceable(1) %p)
    ret void
}

declare void @f(ptr byval(i8))
