; nonnull may return poison
; ERROR: Target is more poisonous than source

define i8* @src() {
  %p = call i8* @f()
  %p.fr = freeze i8* %p
  ret i8* %p.fr
}

define i8* @tgt() {
  %p = call i8* @f()
  ret i8* %p
}

declare nonnull i8* @f()
