@g7 = constant {[0 x i32], [0 x i8], ptr} { [0 x i32] poison, [0 x i8] poison, ptr null }

define ptr @test_leading_zero_size_elems() {
  %v = load ptr, ptr @g7
  ret ptr %v
}


