# ui：画面（LVGL）
# ui.c・nk_theme.c・scr_*.c：主な画面の見た目の試作（値はすべて固定）
# ui_agreement.c・ui_permissions.c・ui_privacy_settings.c・ui_font.c：初回起動の同意と権限の画面（UX-1・Issue #28）
add_library(nk_ui STATIC
    ui.c
    nk_theme.c
    scr_home.c
    scr_record.c
    scr_night.c
    scr_settings.c
    ui_agreement.c
    ui_permissions.c
    ui_privacy_settings.c
    ui_font.c
)
target_include_directories(nk_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(nk_ui PUBLIC lvgl PRIVATE nk_data nk_options)
