find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

if (NOT LLVM_ENABLE_RTTI)
  message(FATAL_ERROR "LLVM must be built with '-DLLVM_ENABLE_RTTI=ON'")
endif()

if (NOT LLVM_ENABLE_EH)
  message(FATAL_ERROR "LLVM must be built with '-DLLVM_ENABLE_EH=ON'")
endif()

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_llvm_loadable_module(tv tv.cpp)

target_link_libraries(tv ${ALIVE_LIBS} ${Z3_LIBRARIES})
