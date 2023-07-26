define {} @src(ptr %p, ptr %q) {
  %v = load {}, ptr %p
  %w = load {}, ptr %q
  ret {} %v
}

define {} @tgt(ptr %p, ptr %q) {
  %v = load {}, ptr %p
  %w = load {}, ptr %q
  ret {} %w
}
