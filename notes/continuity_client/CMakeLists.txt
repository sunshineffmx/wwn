cmake_minimum_required(VERSION 2.8)
project(CHASH)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0") 

set(INDEX_FILES pflush.c continuity_hashing.c hash.c debug.c)

add_library(Indexes ${INDEX_FILES})

set(CLIENT client1.c)
add_executable(client ${CLIENT})

target_link_libraries(client Indexes pthread rdmacm ibverbs m)

