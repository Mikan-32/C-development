# Unity ライブラリを定義
add_library(unity STATIC
    ../third_party/Unity/src/unity.c
    ../third_party/Unity/src/unity.h
)
target_include_directories(unity PUBLIC
    ../third_party/Unity/src
)

# 同意画面のテスト実行ファイルを定義
add_executable(test_ui_agreement
    test_ui_agreement.c
)

target_link_libraries(test_ui_agreement PRIVATE
    unity
    nk_ui
    nk_options
    lvgl
)

# CTest にテスト項目として登録
add_test(NAME test_ui_agreement COMMAND test_ui_agreement)
