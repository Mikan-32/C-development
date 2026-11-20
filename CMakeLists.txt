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
        -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wmissing-prototypes
    )

    if(NK_WARNINGS_AS_ERRORS)
        target_compile_options(nk_options INTERFACE -Werror)
    endif()
endif()

if(NK_ENABLE_SANITIZERS)
    target_compile_options(nk_options INTERFACE
        -fsanitize=address,undefined
        -fno-sanitize-recover=undefined
        -fno-omit-frame-pointer
    )

    target_link_options(nk_options INTERFACE
        -fsanitize=address,undefined
    )
endif()

if(ANDROID)
    # Google Play の 16KB ページサイズ要件への対応
    target_link_options(nk_options INTERFACE
        "LINKER:-z,max-page-size=16384"
    )
endif()

# ------------------------------------------------------------------
# 外部ライブラリ
# 注意点は third_party/README.md を参照
# ------------------------------------------------------------------
# add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)
# add_subdirectory(third_party/lvgl EXCLUDE_FROM_ALL)

add_subdirectory(src/common)
add_subdirectory(src/data)
add_subdirectory(src/sensing)
add_subdirectory(src/session)

# Androidアプリの起動用ライブラリ
if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)

    add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)

    # Android Studioの同期時に、生成前のPCHを参照しないようにする
    set_property(TARGET SDL3-shared PROPERTY
        DISABLE_PRECOMPILE_HEADERS ON
    )

    add_library(main SHARED
        platform/android/c/android_main.c
        platform/android/c/android_bridge.c
        platform/android/c/android_consent.c
        platform/android/c/android_night.c
    )

    target_link_libraries(main PRIVATE
        SDL3::SDL3
        nk_data
        nk_session
        nk_sensing
        nk_options
        log
    )
endif()

# ------------------------------------------------------------------
# テスト（PC 版のみ）
# ------------------------------------------------------------------
if(NK_BUILD_TESTS AND NOT ANDROID)
    enable_testing()

    add_subdirectory(platform/pc)

    add_library(nk_unity STATIC
        third_party/Unity/src/unity.c
    )

    target_include_directories(nk_unity PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/Unity/src"
    )

    if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_definitions(nk_unity PUBLIC
            UNITY_NORETURN=_Noreturn
        )
    endif()

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

    add_executable(test_sensing_replay
        tests/test_sensing_replay.c
    )

    target_link_libraries(test_sensing_replay PRIVATE
        nk_replay
        nk_unity
        nk_options
    )

    add_test(
        NAME sensing_replay
        COMMAND test_sensing_replay
    )

    set_tests_properties(sensing_replay PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_executable(nk_wav_reader_check
        tests/unit/wav_reader_check.c
    )

    target_link_libraries(nk_wav_reader_check PRIVATE
        nk_replay
        nk_unity
        nk_options
    )

    add_test(
        NAME wav_reader
        COMMAND nk_wav_reader_check
    )

    set_tests_properties(wav_reader PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )

    option(NK_ENABLE_LOCAL_WAV_TESTS
        "Enable replay tests using a local recording"
        OFF
    )

    if(NK_ENABLE_LOCAL_WAV_TESTS)
        add_executable(test_wav_sensing_replay
            tests/test_wav_sensing_replay.c
        )

        target_link_libraries(test_wav_sensing_replay PRIVATE
            nk_replay
            nk_unity
            nk_options
        )

        add_test(
            NAME wav_sensing_replay
            COMMAND test_wav_sensing_replay
        )

        set_tests_properties(wav_sensing_replay PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endif() # NK_ENABLE_LOCAL_WAV_TESTS

endif() # NK_BUILD_TESTS AND NOT ANDROID
