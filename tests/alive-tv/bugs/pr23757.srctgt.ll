; https://bugs.llvm.org/show_bug.cgi?id=23757

define zeroext i1 @src(i32* %cell_ptr) {
  %1 = load i32, i32* %cell_ptr, align 4
  %2 = icmp eq i32 %1, 2147483647
  %3 = add nsw i32 %1, 1
  %inc.0 = select i1 %2, i32 -2147483648, i32 %3
  store i32 %inc.0, i32* %cell_ptr, align 4
  %4 = icmp sgt i32 %inc.0, %1
  ret i1 %4
}

define zeroext i1 @tgt(i32* %cell_ptr) {
  %1 = load i32, i32* %cell_ptr, align 4
  %2 = add nsw i32 %1, 1
  store i32 %2, i32* %cell_ptr, align 4
  ret i1 true
}

; ERROR: Value mismatch
