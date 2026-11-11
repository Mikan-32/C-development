add_executable(nemunoki_pc
    main.c
)

target_include_directories(nemunoki_pc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(nemunoki_pc PRIVATE
    nk_ui
    nk_options
    lvgl
    SDL3::SDL3
)
