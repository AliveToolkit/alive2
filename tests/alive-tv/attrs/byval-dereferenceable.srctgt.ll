define void @src(i8* %p) {
    call void @f(i8* %p)
    ret void
}

define void @tgt(i8* %p) {
    call void @f(i8* dereferenceable(1) %p)
    ret void
}

declare void @f(i8* byval(i8))
