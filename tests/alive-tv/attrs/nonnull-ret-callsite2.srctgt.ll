; nonnull may return poison
; ERROR: Target is more poisonous than source

define ptr @src() {
  %p = call ptr @f()
  %p.fr = freeze ptr %p
  ret ptr %p.fr
}

define ptr @tgt() {
  %p = call ptr @f()
  ret ptr %p
}

declare nonnull ptr @f()
