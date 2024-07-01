define i1 @src(ptr %0) {
   %2 = getelementptr i8, ptr %0, i64 1
   %3 = icmp ult ptr %2, %0
   ret i1 %3
}

define i1 @tgt(ptr %0) {
   %ptr = inttoptr i64 -1 to ptr
   %a4_5.not = icmp eq ptr %0, %ptr
   ret i1 %a4_5.not
}
