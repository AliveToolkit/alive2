define void @src() {
	%p = alloca i32
	%p8 = bitcast i32* %p to i8*
	%q = alloca i32
	%q8 = bitcast i32* %q to i8*
	call void @llvm.lifetime.start.p0i8(i64 4, i8* %p8)
	call void @llvm.lifetime.end.p0i8(i64 4, i8* %p8)

	call void @llvm.lifetime.start.p0i8(i64 4, i8* %q8)

	%ip = ptrtoint i32* %p to i64
	%iq = ptrtoint i32* %q to i64
	%guard = icmp eq i64 %ip, %iq
	br i1 %guard, label %COMPARE, label %EXIT

COMPARE:
	%c0 = icmp eq i8* %p8, %q8
	%qgep1 = getelementptr inbounds i8, i8* %q8, i64 1
	%c1 = icmp eq i8* %p8, %qgep1
	call void @g(i1 %c0) ; false
	call void @g(i1 %c1) ; false
	br label %EXIT

EXIT:
	call void @llvm.lifetime.end.p0i8(i64 4, i8* %q8)
	ret void
}

define void @tgt() {
	%p = alloca i32
	%p8 = bitcast i32* %p to i8*
	%q = alloca i32
	%q8 = bitcast i32* %q to i8*
	call void @llvm.lifetime.start.p0i8(i64 4, i8* %p8)
	call void @llvm.lifetime.end.p0i8(i64 4, i8* %p8)

	call void @llvm.lifetime.start.p0i8(i64 4, i8* %q8)

	%ip = ptrtoint i32* %p to i64
	%iq = ptrtoint i32* %q to i64
	%guard = icmp eq i64 %ip, %iq
	br i1 %guard, label %COMPARE, label %EXIT

COMPARE:
	call void @g(i1 false)
	call void @g(i1 false)
	br label %EXIT

EXIT:
	call void @llvm.lifetime.end.p0i8(i64 4, i8* %q8)
	ret void
}

declare i1 @check(i32*, i32*)
declare void @g(i1)

declare void @llvm.lifetime.start.p0i8(i64, i8*)
declare void @llvm.lifetime.end.p0i8(i64, i8*)
