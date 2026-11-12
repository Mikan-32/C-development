# ui：画面（LVGL）。いまはデモ用のデータ（ui_demo.c）で、すべての画面と状態を表示する
add_library(nk_ui STATIC
    ui.c
    nk_theme.c
    ui_icons.c
    ui_widgets.c
    ui_charts.c
    ui_demo.c
    scr_onboarding.c
    scr_home.c
    scr_history.c
    scr_night.c
    scr_bedroom.c
    scr_settings.c
)
target_include_directories(nk_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(nk_ui PUBLIC lvgl PRIVATE nk_options)
