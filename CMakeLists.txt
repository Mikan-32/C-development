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
    # /utf-8：画面の日本語（UTF-8 のソース）を文字化けさせないため。clang-cl では既定と同じ
    target_compile_options(nk_options INTERFACE /W4 /utf-8)
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
# 画面（SDL3＋LVGL）は、まず PC 版で動かす。
# Android 版への組み込みは、android/ の Gradle プロジェクトを作るときに行う（SDL3 は共有ライブラリにする）
if(NOT ANDROID)
    # SDL3：静的ライブラリにしてアプリに入れる（PC 版で DLL を置かずに起動できるように）
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT DEFINED SDL_UNIX_CONSOLE_BUILD)
        # X11 も Wayland も入っていない環境（CI）では、ウィンドウなしの SDL にする。
        # 画面の確認（--shots）と単体テストはウィンドウなしで動く
        find_package(X11 QUIET)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(NK_WAYLAND QUIET wayland-client)
        endif()
        if(NOT X11_FOUND AND NOT NK_WAYLAND_FOUND)
            message(STATUS "X11 と Wayland が見つからないため、SDL をウィンドウなしでビルドします")
            set(SDL_UNIX_CONSOLE_BUILD ON)
        endif()
    endif()
    add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL SYSTEM)

    # LVGL：third_party/lvgl/src の C ファイルを直接ビルドする。
    # 同梱の CMake は C++・アセンブラ・ThorVG も有効にするため使わない。設定は src/ui/lv_conf.h
    file(GLOB_RECURSE NK_LVGL_SOURCES CONFIGURE_DEPENDS
         "${PROJECT_SOURCE_DIR}/third_party/lvgl/src/*.c")
    add_library(lvgl STATIC ${NK_LVGL_SOURCES})
    target_include_directories(lvgl SYSTEM PUBLIC "${PROJECT_SOURCE_DIR}/third_party/lvgl")
    target_include_directories(lvgl PRIVATE "${PROJECT_SOURCE_DIR}/src/ui")
    target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE
                                           LV_KCONFIG_IGNORE)
    if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(lvgl PRIVATE /W0 /utf-8)
    else()
        target_compile_options(lvgl PRIVATE -w)
    endif()
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        # MSVC の cl では、LVGL の可変長引数のマクロに標準どおりのプリプロセッサが要る
        target_compile_options(lvgl PUBLIC /Zc:preprocessor)
    endif()
    if(UNIX)
        target_link_libraries(lvgl PUBLIC m)
    endif()
endif()

# ------------------------------------------------------------------
# コンポーネント（実装を始めたら追加する）
# 各コンポーネントは静的ライブラリ nk_<名前> として定義し、nk_options をリンクする
# ------------------------------------------------------------------
# add_subdirectory(src/common)
if(NOT ANDROID)
    add_subdirectory(src/ui)
    add_subdirectory(src/app)
endif()

# ------------------------------------------------------------------
# テスト（PC 版のみ）
# ------------------------------------------------------------------
if(NK_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_subdirectory(tests)
endif()
