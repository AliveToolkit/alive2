@g7 = constant {[0 x i32], [0 x i8], {}*} { [0 x i32] undef, [0 x i8] undef, {}* null }

define i64* @test_leading_zero_size_elems() {
  %v = load i64*, i64** bitcast ({[0 x i32], [0 x i8], {}*}* @g7 to i64**)
  ret i64* %v
}


