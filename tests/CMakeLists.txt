# 単体テスト（Unity）。PC 版だけでビルドする
add_library(nk_unity STATIC "${PROJECT_SOURCE_DIR}/third_party/Unity/src/unity.c")
target_include_directories(nk_unity SYSTEM PUBLIC "${PROJECT_SOURCE_DIR}/third_party/Unity/src")

# ui：すべての画面と状態を、ウィンドウなしで作って描けるか（落ちないか）を確かめる
add_executable(test_ui_screens unit/ui/test_ui_screens.c)
target_link_libraries(test_ui_screens PRIVATE nk_ui nk_unity nk_options)
add_test(NAME ui.screens COMMAND test_ui_screens)
