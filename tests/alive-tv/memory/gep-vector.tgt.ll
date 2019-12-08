target datalayout = "e-m:m-p:40:64:64:32-i32:32-i16:16-i8:8-n32"

define <2 x i8*> @f(<2 x i8*> %x) {
  %y = getelementptr i8, <2 x i8*> %x, <2 x i64> <i64 1, i64 1>
  ret <2 x i8*> %y
}

%S = type { i32, [ 100 x i32] }

define <2 x i1> @test6b(<2 x i32> %X, <2 x %S*> %P) {
  %a = icmp eq <2 x i32> %X, <i32 -1, i32 -1>
  ret <2 x i1> %a
}

