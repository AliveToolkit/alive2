define ptr @src(ptr %desc) null_pointer_is_valid {
entry:
  %tobool = icmp eq ptr %desc, null
  br i1 %tobool, label %cond.end, label %cond.false

cond.false:
  %p = load ptr, ptr %desc, align 8
  %v = load ptr, ptr %p, align 8
  br label %cond.end

cond.end:
  %node2 = getelementptr inbounds ptr, ptr %desc, i64 0
  %l = load ptr, ptr %node2, align 8
  ret ptr %l
}

define ptr @tgt(ptr %desc) null_pointer_is_valid {
entry:
  %tobool = icmp eq ptr %desc, null
  br i1 %tobool, label %entry.cond.end_crit_edge, label %cond.false

entry.cond.end_crit_edge:
  %.pre = load ptr, ptr null, align 8
  br label %cond.end

cond.false:
  %p = load ptr, ptr %desc, align 8
  br label %cond.end

cond.end:
  %phi = phi ptr [ %.pre, %entry.cond.end_crit_edge ], [ %p, %cond.false ]
  ret ptr %phi
}
