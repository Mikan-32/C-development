# app：アプリの入口。PC 版は build/<プリセット>/nemunoki(.exe) にできる
add_executable(nemunoki main.c)
target_link_libraries(nemunoki PRIVATE nk_ui SDL3::SDL3 nk_options)
set_target_properties(nemunoki PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
