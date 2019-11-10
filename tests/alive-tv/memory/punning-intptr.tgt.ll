target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"

define i8* @int_ptr_punning(i8* %ptr, i64 %n) {
  store i8 1, i8* %ptr
  %poison = getelementptr inbounds i8, i8* null, i64 1
  ret i8* %poison
}
