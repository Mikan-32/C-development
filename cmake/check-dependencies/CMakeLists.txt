cmake_minimum_required(VERSION 3.25)

project(nemunoki_dependency_check LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include("${CMAKE_CURRENT_LIST_DIR}/../SqlCipher.cmake")

add_executable(nk_dependencies_check
    "${CMAKE_CURRENT_LIST_DIR}/../../tests/unit/sqlcipher_check.c"
)

target_link_libraries(nk_dependencies_check PRIVATE
    nk_sqlcipher
)

enable_testing()

add_test(
    NAME sqlcipher_dependency
    COMMAND nk_dependencies_check
)
