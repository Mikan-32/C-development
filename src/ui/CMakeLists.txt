add_library(nk_ui STATIC
    ui_agreement.c
    ui_agreement.h
    ui_permissions.c
    ui_permissions.h
    ui_privacy_settings.c
    ui_privacy_settings.h
    ui_font.c
    ui_font.h
)

target_include_directories(nk_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(nk_ui PUBLIC
    nk_options
    nk_data
    lvgl
)
