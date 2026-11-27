include("${PROJECT_SOURCE_DIR}/cmake/SqlCipher.cmake")
add_library(nk_data STATIC
    data.c
    data_management.c
    data_consent.c
    data_session.c
    nk_db.c
)
target_include_directories(nk_data PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(nk_data PUBLIC nk_options nk_common sqlcipher::sqlcipher)
