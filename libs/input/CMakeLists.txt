project(input)

add_library(input STATIC input.cpp)

target_link_libraries(input pico_stdlib)

target_include_directories(input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})