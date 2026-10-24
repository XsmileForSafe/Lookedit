cmake_minimum_required(VERSION 3.10)
project(lookedit C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR}/src)

set(SRC_FILES
    src/main.c
    src/term.c
    src/buffer.c
    src/fileio.c
    src/cmd.c
)

add_executable(lookedit ${SRC_FILES})

target_compile_options(lookedit PRIVATE -Os)
target_link_options(lookedit PRIVATE -s)
install(TARGETS lookedit RUNTIME DESTINATION bin)
