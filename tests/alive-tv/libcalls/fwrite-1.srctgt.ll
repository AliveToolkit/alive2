target triple = "x86_64-unknown-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, %struct._IO_codecvt*, %struct._IO_wide_data*, %struct._IO_FILE*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type opaque
%struct._IO_codecvt = type opaque
%struct._IO_wide_data = type opaque

@.str = private unnamed_addr constant [5 x i8] c"xpto\00", align 1

define i64 @src(%struct._IO_FILE* %f) {
  %call = call i64 @fwrite(i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str, i64 0, i64 0), i64 1, i64 0, %struct._IO_FILE* %f)
  ret i64 %call
}

define i64 @tgt(%struct._IO_FILE* %f) {
  ret i64 0
}

declare i64 @fwrite(i8*, i64, i64, %struct._IO_FILE*)
