define noundef ptr @calloc(i64 noundef %a, i64 %b) nofree willreturn memory(inaccessiblemem: readwrite) {
  ret ptr null
}
