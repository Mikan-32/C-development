# UI コンポーネントの静的ライブラリを定義
add_library(nk_ui STATIC
    ui_agreement.c
    ui_agreement.h
)

# 外部から nk_ui を使うときにこのフォルダの .h を参照できるようにする
target_include_directories(nk_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 共通オプションと LVGL をリンク
target_link_libraries(nk_ui PUBLIC
    nk_options
    lvgl
)
