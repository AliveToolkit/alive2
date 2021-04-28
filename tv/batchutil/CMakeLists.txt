# add_llvm_loadable_module was removed in https://reviews.llvm.org/D51748
add_llvm_library(tv_batchutil MODULE batchopts.cpp ../utils.cpp)
target_link_libraries(tv_batchutil PRIVATE
  $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>)

if(APPLE)
  set_target_properties(tv_batchutil PROPERTIES
      LINK_FLAGS "-undefined dynamic_lookup"
  )
endif(APPLE)
