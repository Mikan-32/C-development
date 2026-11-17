add_library(nk_sensing STATIC
    nk_sensing.c
    nk_sensing.h
)

target_include_directories(nk_sensing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(nk_sensing PUBLIC
    nk_options
)

# Linux / Android 環境では数学ライブラリ (libm) をリンク
if(UNIX)
    target_link_libraries(nk_sensing PUBLIC m)
endif()
