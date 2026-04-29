project(epdDraw)

add_library(epdDraw STATIC epdDraw.c)
add_subdirectory(epd2in9 epd2in9_BUILDS)

set(FONT_BINARY fonts/unifont.bin)

# Define the path to the binary font file
set(FONT_BINARY_SRC ${CMAKE_CURRENT_SOURCE_DIR}/${FONT_BINARY})
set(FONT_BINARY_DST ${CMAKE_CURRENT_BINARY_DIR}/${FONT_BINARY})

add_custom_command(
    OUTPUT ${FONT_BINARY_DST}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/fonts
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FONT_BINARY_SRC} ${FONT_BINARY_DST}
    DEPENDS ${FONT_BINARY_SRC}
)

set(FONT_OBJ ${CMAKE_CURRENT_BINARY_DIR}/unifont.o)

# Ensure Windows handles objcopy correctly
set(OBJCOPY_TOOL arm-none-eabi-objcopy)

# Convert the binary file into an object file
add_custom_command(
    OUTPUT ${FONT_OBJ}
    COMMAND ${OBJCOPY_TOOL} -I binary -O elf32-littlearm -B arm --rename-section .data=.rodata,alloc,load,readonly,data,contents ${FONT_BINARY} ${FONT_OBJ}
    DEPENDS ${FONT_BINARY_DST}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Create a library from the object file
add_library(font_binary ${FONT_OBJ})
set_target_properties(font_binary PROPERTIES LINKER_LANGUAGE C COMPILE_FLAGS "-ffunction-sections -fdata-sections")
add_dependencies(epdDraw font_binary)

target_link_libraries(epdDraw pico_stdlib epd2in9 font_binary)

target_include_directories(epdDraw PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})