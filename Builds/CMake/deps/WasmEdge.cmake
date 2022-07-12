#[===================================================================[
 NIH dep: wasmedge: web assembly runtime for hooks.
#]===================================================================]

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
add_library (wasmedge STATIC IMPORTED GLOBAL)
ExternalProject_Add (wasmedge_src
  PREFIX ${nih_cache_path}
  GIT_REPOSITORY https://github.com/WasmEdge/WasmEdge.git
  GIT_TAG 0.9.0
    CMAKE_ARGS
  -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
  -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
  $<$<BOOL:${CMAKE_VERBOSE_MAKEFILE}>:-DCMAKE_VERBOSE_MAKEFILE=ON>
  -DCMAKE_DEBUG_POSTFIX=_d
  -DWASMEDGE_BUILD_SHARED_LIB=OFF
  -DWASMEDGE_BUILD_STATIC_LIB=ON
  -DWASMEDGE_BUILD_AOT_RUNTIME=ON
  -DWASMEDGE_FORCE_DISABLE_LTO=ON
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  -DLLVM_DIR=${LLVM_DIR}
  -DLLVM_LIBRARY_DIR=${LLVM_LIBRARY_DIR}
  $<$<NOT:$<BOOL:${is_multiconfig}>>:-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}>
  $<$<BOOL:${MSVC}>:
    "-DCMAKE_C_FLAGS=-GR -Gd -fp:precise -FS -MP"
    "-DCMAKE_C_FLAGS_DEBUG=-MTd"
    "-DCMAKE_C_FLAGS_RELEASE=-MT"
  >
  LOG_BUILD ON
  LOG_CONFIGURE ON
  COMMAND
    pwd
  COMMAND 
    sed -iE 's/uint64_t Index;/uint64_t Index = 0;/g' ../lib/aot/compiler.cpp
  BUILD_COMMAND
    ${CMAKE_COMMAND}
    --build .
    --config $<CONFIG>
    $<$<VERSION_GREATER_EQUAL:${CMAKE_VERSION},3.12>:--parallel ${ep_procs}>
  TEST_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS
      <BINARY_DIR>/lib/api/libwasmedge_c.a
)
ExternalProject_Get_Property (wasmedge_src BINARY_DIR)
set (wasmedge_src_BINARY_DIR "${BINARY_DIR}")
add_dependencies (wasmedge wasmedge_src)
target_include_directories (ripple_libs SYSTEM INTERFACE "${wasmedge_src_BINARY_DIR}/include/api")
set_target_properties (wasmedge PROPERTIES
  IMPORTED_LOCATION_DEBUG
    "${wasmedge_src_BINARY_DIR}/lib/api/libwasmedge_c.a"
  IMPORTED_LOCATION_RELEASE
    "${wasmedge_src_BINARY_DIR}/lib/api/libwasmedge_c.a"
  INTERFACE_INCLUDE_DIRECTORIES
    "${BINARY_DIR}/include/api")
target_link_libraries (ripple_libs INTERFACE wasmedge)
add_library (NIH::WasmEdge ALIAS wasmedge)
