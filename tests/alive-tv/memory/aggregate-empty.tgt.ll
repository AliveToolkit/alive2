@g7 = constant {[0 x i32], [0 x i8], {}*} { [0 x i32] undef, [0 x i8] undef, {}* null }

define i64* @test_leading_zero_size_elems() {
  ret i64* null
}
