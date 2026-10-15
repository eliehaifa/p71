cmake_minimum_required(VERSION 3.22.1)
project(bb C)

add_library(bb SHARED bb.c)

find_library(log-lib log)
target_link_libraries(bb ${log-lib})