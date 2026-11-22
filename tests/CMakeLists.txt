# 単体テスト（Unity）。PC 版だけでビルドする
add_library(nk_unity STATIC "${PROJECT_SOURCE_DIR}/third_party/Unity/src/unity.c")
target_include_directories(nk_unity SYSTEM PUBLIC "${PROJECT_SOURCE_DIR}/third_party/Unity/src")

# ui：すべての画面を、ウィンドウなしで作って描けるか（落ちないか）と、戻る先を確かめる
add_executable(test_ui_screens unit/ui/test_ui_screens.c)
target_link_libraries(test_ui_screens PRIVATE
    nk_ui
    nk_data
    nk_unity
    nk_options
)
add_test(NAME ui.screens COMMAND test_ui_screens)

# ui：同意画面（ON-02）と権限画面（ON-03）の操作と表示
add_executable(test_ui_agreement unit/ui/test_ui_agreement.c)
target_link_libraries(test_ui_agreement PRIVATE nk_ui nk_data nk_unity nk_options)
add_test(NAME ui.agreement COMMAND test_ui_agreement)

# data：同意の保存・版の確認・取り消し
add_executable(test_data_consent unit/data/test_data_consent.c)
target_link_libraries(test_data_consent PRIVATE nk_data nk_unity nk_options)
add_test(NAME data.consent COMMAND test_data_consent)

# data：データベースのキーの確認と、睡眠記録の削除
add_executable(test_nk_db unit/data/test_nk_db.c)
target_link_libraries(test_nk_db PRIVATE nk_data nk_unity nk_options)
add_test(NAME data.db COMMAND test_nk_db)
