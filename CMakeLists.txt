# ねむのき：C 部分のビルド設定（PC 版と Android 版で共通）
# 使い方は README.md の「はじめかた」と、CMakePresets.json を参照
cmake_minimum_required(VERSION 3.25)
project(nemunoki VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NK_BUILD_TESTS "単体テスト（Unity）をビルドする" ON)
option(NK_ENABLE_SANITIZERS "AddressSanitizer と UndefinedBehaviorSanitizer を有効にする（Linux の clang 用）" OFF)
option(NK_WARNINGS_AS_ERRORS "警告をエラーとして扱う（CI では ON）" OFF)

# ------------------------------------------------------------------
# 全コンポーネント共通のコンパイル設定
# 各コンポーネントのターゲットは、これを target_link_libraries でリンクする
# ------------------------------------------------------------------
add_library(nk_options INTERFACE)

if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # clang-cl（Windows）
    target_compile_options(nk_options INTERFACE /W4)
    if(NK_WARNINGS_AS_ERRORS)
        target_compile_options(nk_options INTERFACE /WX)
    endif()
else()
    # clang（Linux・Android）
    target_compile_options(nk_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes)
    if(NK_WARNINGS_AS_ERRORS)
        target_compile_options(nk_options INTERFACE -Werror)
    endif()
endif()

if(NK_ENABLE_SANITIZERS)
    target_compile_options(nk_options INTERFACE
        -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(nk_options INTERFACE -fsanitize=address,undefined)
endif()

if(ANDROID)
    # Google Play の 16KB ページサイズ要件への対応。
    # NDK r28 以降は既定でこの設定になるが、古い NDK で誤ってビルドしたときの保険として明示する
    target_link_options(nk_options INTERFACE "LINKER:-z,max-page-size=16384")
endif()

# ------------------------------------------------------------------
# 外部ライブラリ（third_party に submodule を追加したら有効にする）
# 注意点は third_party/README.md を参照
# ------------------------------------------------------------------
# add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)
# add_subdirectory(third_party/lvgl EXCLUDE_FROM_ALL)

add_subdirectory(src/common)
add_subdirectory(src/data)
add_subdirectory(src/session)

# ------------------------------------------------------------------
# テスト（PC 版のみ）
# ------------------------------------------------------------------
if(NK_BUILD_TESTS AND NOT ANDROID)
    enable_testing()

    add_library(nk_unity STATIC
        third_party/Unity/src/unity.c
    )

    target_include_directories(nk_unity PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Unity/src"
    )

    add_executable(nk_sqlcipher_check
        tests/unit/sqlcipher_check.c
    )

    target_link_libraries(nk_sqlcipher_check PRIVATE
        nk_data
    )

    add_test(
        NAME sqlcipher_check
        COMMAND nk_sqlcipher_check
    )

    add_executable(nk_data_check
        tests/unit/data_check.c
    )

    target_link_libraries(nk_data_check PRIVATE
        nk_data
    )

    add_test(
        NAME data_encryption
        COMMAND nk_data_check
    )

    add_executable(nk_session_check
        tests/unit/session_check.c
    )

    target_link_libraries(nk_session_check PRIVATE
        nk_session
    )

    add_test(
        NAME session_consent
        COMMAND nk_session_check
    )

    add_executable(nk_session_unity_check
        tests/unit/session_unity_check.c
    )

    target_link_libraries(nk_session_unity_check PRIVATE
        nk_session
        nk_unity
    )

    add_test(
        NAME session_unity
        COMMAND nk_session_unity_check
    )
endif()
