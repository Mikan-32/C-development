add_library(nk_session STATIC
    session.c
)

target_include_directories(nk_session PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(nk_session PUBLIC
    nk_options
    nk_common
    nk_data
)
