add_executable(llama-test main.cpp)

#include(BundledLlamaCpp)
#use_bundled_llamacpp(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
target_link_libraries(llama-test llamacpp gsl-lite)