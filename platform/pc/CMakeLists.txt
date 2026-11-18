add_library(nk_replay STATIC
    replay.c
)

target_include_directories(nk_replay PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(nk_replay PUBLIC
    nk_options
    nk_sensing
)
