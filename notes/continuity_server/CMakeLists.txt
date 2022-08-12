cmake_minimum_required(VERSION 2.8)
project(CHASH)

set(INDEX_FILES pflush.c continuity_hashing.c hash.c debug.c)

add_library(Indexes ${INDEX_FILES})

set(SERVER server.c)
add_executable(server ${SERVER})

target_link_libraries(server Indexes pthread rdmacm ibverbs m)
