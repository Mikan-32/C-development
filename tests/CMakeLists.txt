# 単体テスト（Unity）。PC 版だけでビルドする
add_library(nk_unity STATIC "${PROJECT_SOURCE_DIR}/third_party/Unity/src/unity.c")
target_include_directories(nk_unity SYSTEM PUBLIC "${PROJECT_SOURCE_DIR}/third_party/Unity/src")

if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_definitions(nk_unity PUBLIC UNITY_NORETURN=_Noreturn)
endif()
if(NK_BUILD_PC_UI)
# ui：すべての画面を、ウィンドウなしで作って描けるか（落ちないか）と、戻る先を確かめる
add_executable(test_ui_screens unit/ui/test_ui_screens.c)
target_link_libraries(test_ui_screens PRIVATE nk_ui nk_unity nk_options)
add_test(NAME ui.screens COMMAND test_ui_screens)

# ui：同意画面（ON-02）と権限画面（ON-03）の操作と表示
add_executable(test_ui_agreement unit/ui/test_ui_agreement.c)
target_link_libraries(test_ui_agreement PRIVATE nk_ui nk_data nk_unity nk_options)
add_test(NAME ui.agreement COMMAND test_ui_agreement)

endif()
# data：同意の保存・版の確認・取り消し
add_executable(test_data_consent unit/data/test_data_consent.c)
target_link_libraries(test_data_consent PRIVATE nk_data nk_unity nk_options)
add_test(NAME data.consent COMMAND test_data_consent)

# data：データベースのキーの確認と、睡眠記録の削除
add_executable(test_nk_db unit/data/test_nk_db.c)
target_link_libraries(test_nk_db PRIVATE nk_data nk_unity nk_options)
add_test(NAME data.db COMMAND test_nk_db)

# Acquisition, persistence and replay tests share the same SQLCipher and Unity targets.
function(nk_add_core_test target source test_name)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE nk_options nk_unity ${ARGN})
    add_test(NAME ${test_name} COMMAND ${target})
endfunction()
nk_add_core_test(nk_sqlcipher_check unit/sqlcipher_check.c sqlcipher_check nk_data)
nk_add_core_test(nk_data_check unit/data_check.c data_encryption nk_data)
nk_add_core_test(nk_session_check unit/session_check.c session_consent nk_session)
nk_add_core_test(nk_session_unity_check unit/session_unity_check.c session_unity nk_session)
nk_add_core_test(test_sensing_replay test_sensing_replay.c sensing_replay nk_replay)
set_tests_properties(sensing_replay PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
nk_add_core_test(nk_wav_reader_check unit/wav_reader_check.c wav_reader nk_replay)
nk_add_core_test(nk_accel_features_check unit/accel_features_check.c accel_features nk_sensing)
nk_add_core_test(nk_data_recovery_check unit/data_recovery_check.c data_recovery nk_data)
nk_add_core_test(nk_data_integration_check unit/data/test_data_integration.c data.integration nk_session)
if(UNIX)
    nk_add_core_test(nk_recording_consent_check unit/data/test_recording_consent.c recording.consent nk_session nk_sensing)
    target_sources(nk_recording_consent_check PRIVATE "${PROJECT_SOURCE_DIR}/platform/android/c/android_recording.c")
    target_include_directories(nk_recording_consent_check PRIVATE
        "${PROJECT_SOURCE_DIR}/platform/android/c"
        "${PROJECT_SOURCE_DIR}/third_party/SDL/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/support/android-recording")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/recording-consent")
    set_tests_properties(recording.consent PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/recording-consent")
endif()
option(NK_ENABLE_LOCAL_WAV_TESTS "Enable replay using a local private WAV recording" OFF)
if(NK_ENABLE_LOCAL_WAV_TESTS)
    nk_add_core_test(test_wav_sensing_replay test_wav_sensing_replay.c wav_sensing_replay nk_replay)
    set_tests_properties(wav_sensing_replay PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()
