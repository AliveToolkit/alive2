; Found by Alive2

define void @src(ptr %pz, ptr %px, ptr %py) {
  %t2 = load i32, ptr %py
  %t3 = load i32, ptr %px
  %cmp = icmp slt i32 %t2, %t3
  %select = select i1 %cmp, ptr %px, ptr %py
  %r = load i64, ptr %select
  store i64 %r, ptr %pz
  ret void
}

define void @tgt(ptr %pz, ptr %px, ptr %py) {
  %t2 = load i32, ptr %py, align 4
  %t3 = load i32, ptr %px, align 4
  %cmp = icmp slt i32 %t2, %t3
  %r1 = select i1 %cmp, i32 %t3, i32 %t2
  store i32 %r1, ptr %pz, align 4
  ret void
}

; ERROR: Mismatch in memory
