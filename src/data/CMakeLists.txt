# SQLite 本体は外部のコードなので、自分たちの厳しい警告設定（nk_options）は当てない
# （当てると CI の「警告をエラーにする」設定で SQLite 側の警告が止まってしまう）
add_library(nk_sqlite3 STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/sqlite3/sqlite3.c
)

target_include_directories(nk_sqlite3 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/sqlite3
)

add_library(nk_data STATIC
    data_consent.c
    data_consent.h
    data_session.c
    data_session.h
    nk_db.c
    nk_db.h
)

target_include_directories(nk_data PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(nk_data PUBLIC
    nk_options
    nk_sqlite3
)
