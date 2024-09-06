; ERROR: Value mismatch

define ptr @src(ptr %0, i64 %1) {
   %3 = icmp eq i64 %1, 0
   %4 = select i1 %3, ptr null, ptr %0
   ret ptr %4
}

define ptr @tgt(ptr %0, i64 %1) {
   %3 = ptrtoint ptr %0 to i64
   %a2_5 = icmp eq i64 %1, 0
   %a3_6 = select i1 %a2_5, i64 0, i64 %3
   %4 = inttoptr i64 %a3_6 to ptr
   ret ptr %4
}
