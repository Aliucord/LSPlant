cmake_minimum_required(VERSION 3.18.1)
project("lsplant_test")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(test SHARED test.cpp elf_util.cpp xz/xz_crc32.c xz/xz_crc64.c xz/xz_dec_bcj.c xz/xz_dec_lzma2.c xz/xz_dec_stream.c)
find_package(dobby REQUIRED CONFIG)
find_package(lsplant REQUIRED CONFIG)
target_link_libraries(test log dobby::dobby lsplant::lsplant)