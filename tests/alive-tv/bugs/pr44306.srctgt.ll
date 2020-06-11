; Found by Alive2

define void @src(i32* %pz, i32* %px, i32* %py) {
  %t2 = load i32, i32* %py
  %t3 = load i32, i32* %px
  %cmp = icmp slt i32 %t2, %t3
  %select = select i1 %cmp, i32* %px, i32* %py
  %bc = bitcast i32* %select to i64*
  %r = load i64, i64* %bc
  %t1 = bitcast i32* %pz to i64*
  store i64 %r, i64* %t1
  ret void
}

define void @tgt(i32* %pz, i32* %px, i32* %py) {
  %t2 = load i32, i32* %py, align 4
  %t3 = load i32, i32* %px, align 4
  %cmp = icmp slt i32 %t2, %t3
  %r1 = select i1 %cmp, i32 %t3, i32 %t2
  store i32 %r1, i32* %pz, align 4
  ret void
}

; ERROR: Mismatch in memory
