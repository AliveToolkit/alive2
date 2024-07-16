
@a = external global i8

define void @src() {
  %tobool = icmp eq i32 undef, 0
  br i1 %tobool, label %if.end11.loopexit, label %for.cond.preheader

for.cond.preheader:
  unreachable

if.end11.loopexit:
  load i8, ptr @a, align 1
  ret void
}

define void @tgt() {
  unreachable
}
