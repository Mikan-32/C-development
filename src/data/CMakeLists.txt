add_library(nk_data STATIC
    data_consent.c
    data_consent.h
    nk_db.c
    nk_db.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/sqlite3/sqlite3.c
)

target_include_directories(nk_data PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/sqlite3
)

target_link_libraries(nk_data PUBLIC
    nk_options
)
