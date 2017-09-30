cmake_minimum_required(VERSION 2.8.11)

set (CMAKE_CXX_FLAGS "-std=c++14 -O3")
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/googletest-download" )
execute_process(COMMAND "${CMAKE_COMMAND}" --build .
   WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/googletest-download" )

#set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_BINARY_DIR}/googletest-src"
                "${CMAKE_BINARY_DIR}/googletest-build")

project(libzzflate_project)
add_library(zzflate STATIC Encoder.cpp huffman.cpp zzflate.cpp adler.cpp crc.cpp)
install(TARGETS zzflate DESTINATION lib)
install(FILES *.h DESTINATION include)

project(zztest)
add_executable(zztestx zztest/Test.cpp zztest/TestBitOutput.cpp)
find_package(ZLIB)
if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
    target_link_libraries(zztestx ${ZLIB_LIBRARIES})
endif()
target_link_libraries(zztestx zzflate gtest stdc++fs)
install(TARGETS zztestx DESTINATION bin)

