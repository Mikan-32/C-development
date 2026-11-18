add_library(nk_common INTERFACE)

target_include_directories(nk_common
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(nk_common
    INTERFACE
        nk_options
)
