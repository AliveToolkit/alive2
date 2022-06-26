declare ptr @f(ptr)
declare void @g(ptr)

define void @src(ptr %dummy, ptr %operands) {
  %src = alloca ptr, align 8
  %ptr = load ptr, ptr %operands, align 8
  %call = call ptr @f(ptr %ptr)
  call void @g(ptr %call)
  ret void
}

define void @tgt(ptr %dummy, ptr %operands) {
  %ptr = load ptr, ptr %operands, align 8
  %call = call ptr @f(ptr %ptr)
  call void @g(ptr %call)
  ret void
}
