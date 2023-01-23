define void @src(ptr %p, ptr %p2) {
  %a = load ptr, ptr %p, align 1, !nonnull !{}, !noundef !{}
  %b = load ptr, ptr %p2, align 1, !noundef !{}, !nonnull !{}
  ret void
}

define void @tgt(ptr %p, ptr %p2) {
  %a = load ptr, ptr %p, align 1
  %b = load ptr, ptr %p2, align 1
  %c1 = icmp ne ptr %a, null
  %c2 = icmp ne ptr %b, null
  call void @llvm.assume(i1 %c1)
  call void @llvm.assume(i1 %c2)
  ret void
}

declare void @llvm.assume(i1)
