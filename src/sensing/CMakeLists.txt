add_library(nk_sensing STATIC
    sensing.c
    audio_features.c
)

target_include_directories(nk_sensing
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(nk_sensing
    PUBLIC
        nk_options
        nk_common
        nk_data
)

if(UNIX)
    target_link_libraries(nk_sensing
        PRIVATE
            m
    )
endif()

target_sources(nk_sensing PRIVATE accel_features.c)
