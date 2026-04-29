project(epd2in9)

add_library(epd2in9 STATIC epd2in9.c)

target_link_libraries(epd2in9 pico_stdlib hardware_spi)

target_include_directories(epd2in9 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})