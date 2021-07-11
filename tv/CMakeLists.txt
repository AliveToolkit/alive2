# add_llvm_loadable_module was removed in https://reviews.llvm.org/D51748
if (COMMAND add_llvm_loadable_module)
  add_llvm_loadable_module(tv tv.cpp)
else()
  add_llvm_library(tv MODULE tv.cpp)
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/scripts/alivecc.in
  ${CMAKE_BINARY_DIR}/alivecc
  @ONLY
)
configure_file(
  ${CMAKE_SOURCE_DIR}/scripts/alivecc.in
  ${CMAKE_BINARY_DIR}/alive++
  @ONLY
)
configure_file(
  ${CMAKE_SOURCE_DIR}/scripts/benchmark-clang-tv.in
  ${CMAKE_BINARY_DIR}/benchmark-clang-tv
  @ONLY
)
configure_file(
  ${CMAKE_SOURCE_DIR}/scripts/benchmark-llvm-test-suite.in
  ${CMAKE_BINARY_DIR}/benchmark-llvm-test-suite
  @ONLY
)

target_link_libraries(tv PRIVATE ${ALIVE_LIBS_LLVM} ${Z3_LIBRARIES}
  $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>)

if(APPLE)
    set_target_properties(tv PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)
