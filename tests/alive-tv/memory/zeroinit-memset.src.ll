; From Transforms/MemCpyOpt/fca2memcpy.ll
target datalayout = "e-i64:64-f80:128-n8:16:32:64"

%S = type { i8*, i8, i32 }

define void @destroysrc(%S* %src, %S* %dst) {
  %1 = load %S, %S* %src
  store %S zeroinitializer, %S* %src
  store %S %1, %S* %dst
  ret void
}


