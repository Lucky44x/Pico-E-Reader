project(microMD)

add_library(microMD STATIC microMD.c)

target_link_libraries(microMD pico_stdlib)
target_link_libraries(microMD pico_fatfs)

target_include_directories(microMD PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})