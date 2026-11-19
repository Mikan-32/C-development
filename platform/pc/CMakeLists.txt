# 初回起動の同意と権限の画面（UX-1・Issue #28）を、単独で動かして確かめる PC 版
# インクルードの場所は nk_ui・nk_data・lvgl のターゲットから受け取る
# （LVGL の設定は src/ui/lv_conf.h の 1 つだけ。別の lv_conf.h が見えないようにする）
add_executable(nemunoki_pc
    main.c
)

target_link_libraries(nemunoki_pc PRIVATE
    nk_ui
    nk_data
    nk_options
    SDL3::SDL3
)
