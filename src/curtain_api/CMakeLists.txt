add_library(nk_curtain_api STATIC
    curtain_api.c
    curtain_api.h
)
target_include_directories(nk_curtain_api PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

if(TARGET cjson)
    target_link_libraries(nk_curtain_api PRIVATE cjson)
else()
    target_sources(nk_curtain_api PRIVATE "${PROJECT_SOURCE_DIR}/third_party/cJSON/cJSON.c")
    target_include_directories(nk_curtain_api SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/cJSON")
endif()

if(TARGET CURL::libcurl)
    target_link_libraries(nk_curtain_api PRIVATE CURL::libcurl)
elseif(TARGET libcurl)
    target_link_libraries(nk_curtain_api PRIVATE libcurl)
endif()

if(TARGET OpenSSL::Crypto)
    target_link_libraries(nk_curtain_api PRIVATE OpenSSL::Crypto)
endif()

target_link_libraries(nk_curtain_api PUBLIC nk_common PRIVATE nk_options)
