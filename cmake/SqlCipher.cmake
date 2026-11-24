include_guard(GLOBAL)

get_filename_component(_nk_repo_root
    "${CMAKE_CURRENT_LIST_DIR}/.."
    ABSOLUTE
)

file(READ
    "${_nk_repo_root}/cmake/dependencies.json"
    _nk_dependencies
)

string(JSON _nk_cipher_version
    GET "${_nk_dependencies}" sqlcipher version
)

string(JSON _nk_cipher_commit
    GET "${_nk_dependencies}" sqlcipher commit
)

string(JSON _nk_openssl_version
    GET "${_nk_dependencies}" openssl version
)

string(JSON _nk_openssl_source_hash
    GET "${_nk_dependencies}" openssl source_sha256
)

set(_nk_cipher_dir
    "${_nk_repo_root}/third_party/prebuilt/sqlcipher"
)

foreach(_nk_file IN ITEMS
    sqlite3.c
    sqlite3.h
    source-commit.txt
)
    if(NOT EXISTS "${_nk_cipher_dir}/${_nk_file}")
        message(FATAL_ERROR
            "SQLCipher source is missing. Run scripts/generate-sqlcipher.cmd."
        )
    endif()
endforeach()

file(READ
    "${_nk_cipher_dir}/source-commit.txt"
    _nk_actual_commit
)

string(STRIP "${_nk_actual_commit}" _nk_actual_commit)

if(NOT "${_nk_actual_commit}" STREQUAL "${_nk_cipher_commit}")
    message(FATAL_ERROR
        "SQLCipher source commit does not match dependencies.json."
    )
endif()

file(STRINGS
    "${_nk_cipher_dir}/sqlite3.c"
    _nk_version_line
    REGEX "^#define CIPHER_VERSION_NUMBER[ \t]+"
)

string(REGEX REPLACE
    ".*CIPHER_VERSION_NUMBER[ \t]+([0-9.]+).*"
    "\\1"
    _nk_actual_version
    "${_nk_version_line}"
)

if(NOT "${_nk_actual_version}" STREQUAL "${_nk_cipher_version}")
    message(FATAL_ERROR
        "Generated SQLCipher version does not match dependencies.json."
    )
endif()

if(ANDROID)
    if(NOT ANDROID_ABI MATCHES "^(arm64-v8a|x86_64)$")
        message(FATAL_ERROR "Unsupported Android ABI: ${ANDROID_ABI}")
    endif()

    set(_nk_platform "android-${ANDROID_ABI}")
    set(_nk_configuration "release")
    set(_nk_architecture "${ANDROID_ABI}")
    set(_nk_library_extension "a")
elseif(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "An x64 compiler is required.")
    endif()

    if(CMAKE_CONFIGURATION_TYPES)
        message(FATAL_ERROR "Use the Ninja single-configuration generator.")
    endif()

    string(TOLOWER "${CMAKE_BUILD_TYPE}" _nk_build_type)

    if(_nk_build_type STREQUAL "debug")
        set(_nk_configuration "debug")
        set(_nk_runtime "MDd")
    elseif(_nk_build_type MATCHES "^(release|relwithdebinfo|minsizerel)$")
        set(_nk_configuration "release")
        set(_nk_runtime "MD")
    else()
        message(FATAL_ERROR "Set CMAKE_BUILD_TYPE to Debug or Release.")
    endif()

    set(_nk_platform "windows-x64")
    set(_nk_architecture "x64")
    set(_nk_library_extension "lib")
else()
    # Linux CI uses the distribution OpenSSL package, per ADR-0013.
    find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto SSL)

    if(NOT OPENSSL_VERSION MATCHES "^3\\.")
        message(FATAL_ERROR "OpenSSL 3.x is required.")
    endif()
endif()

if(WIN32 OR ANDROID)
    set(_nk_openssl_dir
        "${_nk_repo_root}/third_party/prebuilt/openssl/${_nk_openssl_version}/${_nk_platform}/${_nk_configuration}"
    )

    if(NOT EXISTS "${_nk_openssl_dir}/build-info.json")
        message(FATAL_ERROR
            "Prepare OpenSSL first. Missing: ${_nk_openssl_dir}/build-info.json"
        )
    endif()

    file(READ
        "${_nk_openssl_dir}/build-info.json"
        _nk_build_info
    )

    string(JSON _nk_package_version
        GET "${_nk_build_info}" openssl_version
    )

    string(JSON _nk_package_source_hash
        GET "${_nk_build_info}" source_sha256
    )

    string(JSON _nk_package_architecture
        GET "${_nk_build_info}" architecture
    )

    string(JSON _nk_package_configuration
        GET "${_nk_build_info}" configuration
    )

    string(JSON _nk_package_static
        GET "${_nk_build_info}" static
    )

    if(NOT "${_nk_package_version}" STREQUAL "${_nk_openssl_version}"
        OR NOT "${_nk_package_source_hash}" STREQUAL "${_nk_openssl_source_hash}"
        OR NOT "${_nk_package_architecture}" STREQUAL "${_nk_architecture}"
        OR NOT "${_nk_package_configuration}" STREQUAL "${_nk_configuration}"
        OR NOT _nk_package_static)
        message(FATAL_ERROR "OpenSSL package metadata does not match.")
    endif()

    if(WIN32)
        string(JSON _nk_package_runtime
            GET "${_nk_build_info}" runtime
        )

        if(NOT "${_nk_package_runtime}" STREQUAL "${_nk_runtime}")
            message(FATAL_ERROR "OpenSSL runtime does not match the build configuration.")
        endif()
    endif()

    foreach(_nk_component IN ITEMS Crypto SSL)
        string(TOLOWER "${_nk_component}" _nk_component_lower)

        set(_nk_library_path
            "${_nk_openssl_dir}/lib/lib${_nk_component_lower}.${_nk_library_extension}"
        )

        string(JSON _nk_expected_hash
            GET "${_nk_build_info}" "${_nk_component_lower}_sha256"
        )

        if(NOT EXISTS "${_nk_library_path}")
            message(FATAL_ERROR "Missing library: ${_nk_library_path}")
        endif()

        file(SHA256 "${_nk_library_path}" _nk_actual_hash)

        if(NOT "${_nk_actual_hash}" STREQUAL "${_nk_expected_hash}")
            message(FATAL_ERROR "Library SHA-256 mismatch: ${_nk_library_path}")
        endif()

        add_library(OpenSSL::${_nk_component} STATIC IMPORTED GLOBAL)

        set_target_properties(OpenSSL::${_nk_component} PROPERTIES
            IMPORTED_LOCATION "${_nk_library_path}"
            INTERFACE_INCLUDE_DIRECTORIES "${_nk_openssl_dir}/include"
        )
    endforeach()

    set_property(TARGET OpenSSL::SSL APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
    )

    if(WIN32)
        set_property(TARGET OpenSSL::Crypto APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES
                ws2_32
                gdi32
                advapi32
                crypt32
                user32
        )
    endif()
endif()

set(_nk_cipher_include
    "${CMAKE_CURRENT_BINARY_DIR}/generated-sqlcipher"
)

file(MAKE_DIRECTORY "${_nk_cipher_include}/sqlcipher")

configure_file(
    "${_nk_cipher_dir}/sqlite3.h"
    "${_nk_cipher_include}/sqlcipher/sqlite3.h"
    COPYONLY
)

add_library(nk_sqlcipher STATIC
    "${_nk_cipher_dir}/sqlite3.c"
)

add_library(sqlcipher::sqlcipher ALIAS nk_sqlcipher)

set_target_properties(nk_sqlcipher PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    POSITION_INDEPENDENT_CODE YES
)

target_include_directories(nk_sqlcipher PUBLIC
    "${_nk_cipher_include}"
    "${_nk_cipher_dir}"
)

target_compile_definitions(nk_sqlcipher
    PUBLIC
        SQLITE_HAS_CODEC
    PRIVATE
        SQLITE_TEMP_STORE=2
        SQLITE_THREADSAFE=1
        SQLCIPHER_CRYPTO_OPENSSL
        SQLITE_EXTRA_INIT=sqlcipher_extra_init
        SQLITE_EXTRA_SHUTDOWN=sqlcipher_extra_shutdown
        SQLCIPHER_OMIT_DLLMAIN
)

target_link_libraries(nk_sqlcipher PRIVATE
    OpenSSL::Crypto
)

if(UNIX)
    find_package(Threads REQUIRED)

    target_link_libraries(nk_sqlcipher PRIVATE
        Threads::Threads
        ${CMAKE_DL_LIBS}
        m
    )

    if(ANDROID)
        find_library(_nk_android_log log REQUIRED)
        target_link_libraries(nk_sqlcipher PRIVATE "${_nk_android_log}")
    endif()
endif()

message(STATUS "SQLCipher source version: ${_nk_cipher_version}")
