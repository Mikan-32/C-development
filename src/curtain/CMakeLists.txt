add_library(nk_curtain STATIC
    curtain.c
    curtain.h
)
target_include_directories(nk_curtain PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(nk_curtain
    PUBLIC nk_curtain_api nk_common
    PRIVATE nk_data SDL3::SDL3 nk_options
)
