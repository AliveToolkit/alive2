define i1 @f(ptr %a) {
  %load = load ptr, ptr %a
  %integral = ptrtoint ptr %load to i64
  %cmp = icmp slt i64 %integral, 0
  ret i1 %cmp
}
