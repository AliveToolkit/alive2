define i1 @f(ptr %a) {
  %load = load ptr, ptr %a, align 8
  %cmp = icmp slt ptr %load, null
  ret i1 %cmp
}

