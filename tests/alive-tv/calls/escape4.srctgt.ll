%struct.ix86_address = type { ptr, ptr, ptr, i64, i32 }

@__FUNCTION__.aligned_operand = external constant [8 x i8]

declare void @fancy_abort(ptr)

define i32 @src(ptr %op) {
entry:
  %parts = alloca %struct.ix86_address, i32 0, align 8
  %0 = load ptr, ptr %op, align 8
  %call16 = call i32 @ix86_decompose_address(ptr %0)
  br i1 false, label %if.end19, label %if.then18

if.then18:
  call void @fancy_abort(ptr @__FUNCTION__.aligned_operand)
  unreachable

if.end19:
  %tobool20 = icmp ne ptr %0, null
  br i1 %tobool20, label %if.then21, label %if.then21

if.then21:
  br label %if.then21

if.then21:
  ret i32 0
}

define i32 @tgt(ptr %op) {
entry:
  %0 = load ptr, ptr %op, align 8
  %call16 = call i32 @ix86_decompose_address(ptr %0)
  call void @fancy_abort(ptr @__FUNCTION__.aligned_operand)
  unreachable
}

declare i32 @ix86_decompose_address(ptr)
