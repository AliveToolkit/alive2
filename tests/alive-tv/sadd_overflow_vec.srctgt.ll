define <2 x i1> @src() {
	%t = call { <2 x i32>, <2 x i1> } @llvm.sadd.with.overflow.v2i32(<2 x i32> <i32 2147483647, i32 2147483647>, <2 x i32> <i32 1, i32 1>)
	%v = extractvalue { <2 x i32>, <2 x i1> } %t, 1
	ret <2 x i1> %v
}

define <2 x i1> @tgt() {
	ret <2 x i1> <i1 1, i1 1>
}

declare { <2 x i32>, <2 x i1> } @llvm.sadd.with.overflow.v2i32(<2 x i32>, <2 x i32>)
