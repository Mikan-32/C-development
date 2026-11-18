include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/SqlCipher.cmake")

add_library(nk_data STATIC
    data.c
)

target_include_directories(nk_data PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(nk_data PRIVATE
    SQLITE_HAS_CODEC
)

target_link_libraries(nk_data PUBLIC
    nk_options
    nk_common
    sqlcipher::sqlcipher
)
