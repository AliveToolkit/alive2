; Show that storing {} is a nop

define void @src(ptr noundef %p) {
  ret void
}

define void @tgt(ptr noundef %p) {
  store {} {}, ptr %p
  ret void
}
