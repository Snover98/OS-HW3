cmake_minimum_required(VERSION 3.7)
project(OS_HW3)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        Factory.cxx
        Factory.h
        Product.h test_din.cpp)

add_executable(OS_HW3 ${SOURCE_FILES})