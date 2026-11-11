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

# データ層の単体テスト実行ファイル
add_executable(test_data_consent
    test_data_consent.c
)

target_link_libraries(test_data_consent PRIVATE
    unity
    nk_data
    nk_options
)

add_test(NAME test_data_consent COMMAND test_data_consent)

add_executable(test_nk_db
    test_nk_db.c
)

target_link_libraries(test_nk_db PRIVATE
    unity
    nk_data
    nk_options
)

add_test(NAME test_nk_db COMMAND test_nk_db)
