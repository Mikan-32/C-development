# ui：画面（LVGL）。値はすべて固定の、見た目の試作
add_library(nk_ui STATIC
    ui.c
    nk_theme.c
    scr_home.c
    scr_record.c
    scr_night.c
    scr_settings.c
)
target_include_directories(nk_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(nk_ui PUBLIC lvgl PRIVATE nk_options)
