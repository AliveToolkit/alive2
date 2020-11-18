define {} @src({}* %p, {}* %q) {
  %v = load {}, {}* %p
  %w = load {}, {}* %q
  ret {} %v
}

define {} @tgt({}* %p, {}* %q) {
  %v = load {}, {}* %p
  %w = load {}, {}* %q
  ret {} %w
}
