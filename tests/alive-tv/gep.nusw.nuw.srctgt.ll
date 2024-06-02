target datalayout = "p:8:8"

define ptr @src_refine_inbounds_to_nusw(ptr %x, i8 %i) {
  %p = getelementptr inbounds i8, ptr %x, i8 %i
  ret ptr %p
}

define ptr @tgt_refine_inbounds_to_nusw(ptr %x, i8 %i) {
  %p = getelementptr nusw i8, ptr %x, i8 %i
  ret ptr %p
}

define ptr @src_nuw_implies_trunc_nuw(ptr %x, i16 %i) {
  %p = getelementptr nuw i8, ptr %x, i16 %i
  ret ptr %p
}

define ptr @tgt_nuw_implies_trunc_nuw(ptr %x, i16 %i) {
  %i.trunc = trunc nuw i16 %i to i8
  %p = getelementptr nuw i8, ptr %x, i8 %i.trunc
  ret ptr %p
}

define ptr @src_nusw_implies_trunc_nsw(ptr %x, i16 %i) {
  %p = getelementptr nusw i8, ptr %x, i16 %i
  ret ptr %p
}

define ptr @tgt_nusw_implies_trunc_nsw(ptr %x, i16 %i) {
  %i.trunc = trunc nsw i16 %i to i8
  %p = getelementptr nusw i8, ptr %x, i8 %i.trunc
  ret ptr %p
}

define ptr @src_nuw_implies_mul_nuw(ptr %x, i8 %i) {
  %p = getelementptr nuw i16, ptr %x, i8 %i
  ret ptr %p
}

define ptr @tgt_nuw_implies_mul_nuw(ptr %x, i8 %i) {
  %i.mul = mul nuw i8 %i, 2
  %p = getelementptr nuw i8, ptr %x, i8 %i.mul
  ret ptr %p
}

define ptr @src_nusw_implies_mul_nsw(ptr %x, i8 %i) {
  %p = getelementptr nusw i16, ptr %x, i8 %i
  ret ptr %p
}

define ptr @tgt_nusw_implies_mul_nsw(ptr %x, i8 %i) {
  %i.mul = mul nsw i8 %i, 2
  %p = getelementptr nusw i8, ptr %x, i8 %i.mul
  ret ptr %p
}

define ptr @src_nuw_implies_add_nuw(ptr %x, i8 %i, i8 %j) {
  %p = getelementptr nuw [1 x i8], ptr %x, i8 %i, i8 %j
  ret ptr %p
}

define ptr @tgt_nuw_implies_add_nuw(ptr %x, i8 %i, i8 %j) {
  %i.j = add nuw i8 %i, %j
  %p = getelementptr nuw i8, ptr %x, i8 %i.j
  ret ptr %p
}

define i8 @src_nuw_implies_ptr_add_nuw(ptr %x, i8 %i) {
  %p = getelementptr nuw i8, ptr %x, i8 %i
  %p.i = ptrtoint ptr %p to i8
  ret i8 %p.i
}

define i8 @tgt_nuw_implies_ptr_add_nuw(ptr %x, i8 %i) {
  %x.i = ptrtoint ptr %x to i8
  %p.i = add nuw i8 %x.i, %i
  ret i8 %p.i
}

define ptr @src_nusw_implies_add_nsw(ptr %x, i8 %i, i8 %j) {
  %p = getelementptr nusw [1 x i8], ptr %x, i8 %i, i8 %j
  ret ptr %p
}

define ptr @tgt_nusw_implies_add_nsw(ptr %x, i8 %i, i8 %j) {
  %i.j = add nsw i8 %i, %j
  %p = getelementptr nusw i8, ptr %x, i8 %i.j
  ret ptr %p
}

define i8 @src_nusw_implies_ptr_add_nusw(ptr %x, i8 %i) {
  %p = getelementptr nusw i8, ptr %x, i8 %i
  %p.i = ptrtoint ptr %p to i8
  ret i8 %p.i
}

define i8 @tgt_nusw_implies_ptr_add_nusw(ptr %x, i8 %i) {
  %x.i = ptrtoint ptr %x to i8
  %x.i.e = zext i8 %x.i to i16
  %i.e = sext i8 %i to i16
  %p.i.e = add i16 %x.i.e, %i.e
  %p.i = trunc nuw i16 %p.i.e to i8
  ret i8 %p.i
}

define i1 @src_nuw_implies_uge(ptr %x, i8 %i) {
  %p = getelementptr nuw i8, ptr %x, i8 %i
  %c = icmp uge ptr %p, %x
  ret i1 %c
}

define i1 @tgt_nuw_implies_uge(ptr %x, i8 %i) {
  ret i1 true
}

define void @src_nusw_nuw_implies_nonneg(ptr %x, i8 %i) {
  %p = getelementptr nusw nuw i8, ptr %x, i8 %i
  %c = icmp sge i8 %i, 0
  store i1 %c, ptr %p
  ret void
}

define void @tgt_nusw_nuw_implies_nonneg(ptr %x, i8 %i) {
  %p = getelementptr nusw nuw i8, ptr %x, i8 %i
  store i1 true, ptr %p
  ret void
}
