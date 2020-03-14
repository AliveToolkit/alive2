; https://bugs.llvm.org/show_bug.cgi?id=31808 , with minor fixes
%struct.SomeStruct = type { %struct.Array }
%struct.Array = type { i8*, i32, i32 }
@.str = private unnamed_addr constant [11 x i8] c"{{BEGIN}}\0A\00", align 1
@.str.1 = private unnamed_addr constant [9 x i8] c"{{END}}\0A\00", align 1
@.str.2 = private unnamed_addr constant [12 x i8] c"%p %p {%s}\0A\00", align 1
@.str.3 = private unnamed_addr constant [5 x i8] c"PASS\00", align 1
@.str.4 = private unnamed_addr constant [5 x i8] c"FAIL\00", align 1

define linkonce_odr void @src(%struct.SomeStruct*) local_unnamed_addr {
  %2 = getelementptr inbounds %struct.SomeStruct, %struct.SomeStruct* %0, i64 0, i32 0
  %3 = bitcast %struct.Array* %2 to i8**
  %4 = load i8*, i8** %3, align 8, !tbaa !8
  %5 = bitcast i8* %4 to i8*
  %6 = getelementptr inbounds %struct.Array, %struct.Array* %2, i64 0, i32 2
  %7 = load i32, i32* %6, align 4, !tbaa !2
  %8 = zext i32 %7 to i64
  %9 = shl nuw nsw i64 %8, 3
  %10 = getelementptr inbounds i8, i8* %5, i64 %9
  %11 = bitcast i8* %10 to i32**
  %12 = bitcast i8* %4 to i32**
  %13 = getelementptr inbounds i32*, i32** %12, i64 1
  %14 = icmp eq i32** %13, %11
  %15 = select i1 %14, [5 x i8]* @.str.3, [5 x i8]* @.str.4
  %16 = getelementptr inbounds [5 x i8], [5 x i8]* %15, i64 0, i64 0
  %17 = call i32 (i8*, ...) @f(i8* getelementptr inbounds ([12 x i8], [12 x i8]* @.str.2, i64 0, i64 0), i32** %13, i32** %11, i8* %16)
  ret void
}

define linkonce_odr void @tgt(%struct.SomeStruct*) local_unnamed_addr {
  %2 = bitcast %struct.SomeStruct* %0 to i8**
  %3 = load i8*, i8** %2, align 8, !tbaa !8
  %4 = getelementptr inbounds %struct.SomeStruct, %struct.SomeStruct* %0, i64 0, i32 0, i32 2
  %5 = load i32, i32* %4, align 4, !tbaa !2
  %6 = zext i32 %5 to i64
  %7 = shl nuw nsw i64 %6, 3
  %8 = bitcast i8* %3 to i8*
  %9 = getelementptr inbounds i8, i8* %8, i64 %7
  %10 = bitcast i8* %3 to i32**
  %11 = getelementptr inbounds i32*, i32** %10, i64 1
  %12 = call i32 (i8*, ...) @f(i8* getelementptr inbounds ([12 x i8], [12 x i8]* @.str.2, i64 0, i64 0), i32** %11, i8* %9, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.4, i64 0, i64 0))
  ret void
}

declare i32 @f(i8* nocapture readonly, ...) local_unnamed_addr
!0 = !{i32 1, !"PIC Level", i32 2}
!1 = !{!"clang version 3.9.0 (tags/RELEASE_390/final)"}
!2 = !{!3, !4, i64 0}
!3 = !{!"_ZTS5Array", !4, i64 0, !7, i64 8, !7, i64 12}
!4 = !{!"any pointer", !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C++ TBAA"}
!7 = !{!"int", !5, i64 0}
!8 = !{!3, !7, i64 12}
!9 = !{!7, !7, i64 0}

; ERROR: Source is more defined than target
