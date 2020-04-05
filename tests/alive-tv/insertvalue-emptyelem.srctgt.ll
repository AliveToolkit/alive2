define { {}, { float }, [0 x i8] } @src({ float } %unwrap1) {
  %wrap2 = insertvalue { {}, { float }, [0 x i8] } undef, { float } %unwrap1, 1
  ret { {}, { float }, [0 x i8] } %wrap2
}

define { {}, { float }, [0 x i8] } @tgt({ float } %unwrap1) {
  %wrap2 = insertvalue { {}, { float }, [0 x i8] } undef, { float } %unwrap1, 1
  ret { {}, { float }, [0 x i8] } %wrap2
}
