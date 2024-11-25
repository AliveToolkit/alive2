add_llvm_library(tv MODULE tv.cpp)

configure_file(
  ${PROJECT_SOURCE_DIR}/scripts/alivecc.in
  ${PROJECT_BINARY_DIR}/alivecc
  @ONLY
)
configure_file(
  ${PROJECT_SOURCE_DIR}/scripts/alivecc.in
  ${PROJECT_BINARY_DIR}/alive++
  @ONLY
)
configure_file(
  ${PROJECT_SOURCE_DIR}/scripts/benchmark-clang-tv.in
  ${PROJECT_BINARY_DIR}/benchmark-clang-tv
  @ONLY
)
configure_file(
  ${PROJECT_SOURCE_DIR}/scripts/benchmark-llvm-test-suite.in
  ${PROJECT_BINARY_DIR}/benchmark-llvm-test-suite
  @ONLY
)

target_link_libraries(tv PRIVATE ${ALIVE_LIBS_LLVM} ${Z3_LIBRARIES} ${HIREDIS_LIBRARIES})

if(APPLE)
    set_target_properties(tv PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)
