define noundef ptr @calloc(i64 noundef %a, i64 %b) nofree willreturn inaccessiblememonly {
  ret ptr null
}
