# add_llvm_loadable_module was removed in https://reviews.llvm.org/D51748
if (COMMAND add_llvm_loadable_module)
  add_llvm_loadable_module(tv tv.cpp)
else()
  add_llvm_library(tv MODULE tv.cpp)
endif()

if (CLANG_PLUGIN)
  message(STATUS "Clang plugin support enabled")
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
    ${CMAKE_SOURCE_DIR}/scripts/benchmark_clang_tv.in
    ${CMAKE_BINARY_DIR}/benchmark_clang_tv
    @ONLY
  )
else()
  message(STATUS "Skipping Clang plugin")
endif()

target_link_libraries(tv PRIVATE ${ALIVE_LIBS_LLVM} ${Z3_LIBRARIES}
  $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>)

if(APPLE)
    set_target_properties(tv PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)
