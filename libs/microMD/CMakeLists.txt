project(microMD)

add_library(microMD STATIC microMD.c)

target_link_libraries(microMD pico_stdlib)

target_include_directories(epdDraw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})