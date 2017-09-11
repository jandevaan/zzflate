set (CMAKE_CXX_FLAGS "-std=c++14")

cmake_minimum_required(VERSION 2.8)
project(libtest_project)
add_library(test STATIC Encoder.cpp huffman.cpp zzflate.cpp)
install(TARGETS test DESTINATION lib)
install(FILES *.h DESTINATION include)
