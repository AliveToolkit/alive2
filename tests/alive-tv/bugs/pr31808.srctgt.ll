; https://bugs.llvm.org/show_bug.cgi?id=31808 , with minor fixes
%struct.SomeStruct = type { %struct.Array }
%struct.Array = type { ptr, i32, i32 }
@.str = private unnamed_addr constant [11 x i8] c"{{BEGIN}}\0A\00", align 1
@.str.1 = private unnamed_addr constant [9 x i8] c"{{END}}\0A\00", align 1
@.str.2 = private unnamed_addr constant [12 x i8] c"%p %p {%s}\0A\00", align 1
@.str.3 = private unnamed_addr constant [5 x i8] c"PASS\00", align 1
@.str.4 = private unnamed_addr constant [5 x i8] c"FAIL\00", align 1

define void @src(ptr) {
  %2 = getelementptr inbounds %struct.SomeStruct, ptr %0, i64 0, i32 0
  %3 = bitcast ptr %2 to ptr
  %4 = load ptr, ptr %3, align 8
  %5 = bitcast ptr %4 to ptr
  %6 = getelementptr inbounds %struct.Array, ptr %2, i64 0, i32 2
  %7 = load i32, ptr %6, align 4
  %8 = zext i32 %7 to i64
  %9 = shl nuw nsw i64 %8, 3
  %10 = getelementptr inbounds i8, ptr %5, i64 %9
  %11 = bitcast ptr %10 to ptr
  %12 = bitcast ptr %4 to ptr
  %13 = getelementptr inbounds ptr, ptr %12, i64 1
  %14 = icmp eq ptr %13, %11
  %15 = select i1 %14, ptr @.str.3, ptr @.str.4
  %16 = getelementptr inbounds [5 x i8], ptr %15, i64 0, i64 0
  %17 = call i32 (ptr, ...) @f(ptr getelementptr inbounds ([12 x i8], ptr @.str.2, i64 0, i64 0), ptr %13, ptr %11, ptr %16)
  ret void
}

define void @tgt(ptr) {
  %2 = bitcast ptr %0 to ptr
  %3 = load ptr, ptr %2, align 8
  %4 = getelementptr inbounds %struct.SomeStruct, ptr %0, i64 0, i32 0, i32 2
  %5 = load i32, ptr %4, align 4
  %6 = zext i32 %5 to i64
  %7 = shl nuw nsw i64 %6, 3
  %8 = bitcast ptr %3 to ptr
  %9 = getelementptr inbounds i8, ptr %8, i64 %7
  %10 = bitcast ptr %3 to ptr
  %11 = getelementptr inbounds ptr, ptr %10, i64 1
  %12 = call i32 (ptr, ...) @f(ptr getelementptr inbounds ([12 x i8], ptr @.str.2, i64 0, i64 0), ptr %11, ptr %9, ptr getelementptr inbounds ([5 x i8], ptr @.str.4, i64 0, i64 0))
  ret void
}

declare i32 @f(ptr nocapture readonly, ...) local_unnamed_addr

; ERROR: Source is more defined than target
