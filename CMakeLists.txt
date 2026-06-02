cmake_minimum_required(VERSION 3.20)
project(SpeechApp VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# ── spdlog ────────────────────────────────────────────────────────────────────
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.14.1 GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(spdlog)
endif()

# ── nlohmann/json ─────────────────────────────────────────────────────────────
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3 GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ── tinyxml2 ──────────────────────────────────────────────────────────────────
pkg_check_modules(TINYXML2 QUIET IMPORTED_TARGET tinyxml2)
if(NOT TINYXML2_FOUND)
    include(FetchContent)
    FetchContent_Declare(tinyxml2
        GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
        GIT_TAG        10.0.0 GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(tinyxml2)
    add_library(PkgConfig::TINYXML2 ALIAS tinyxml2)
endif()

# ── SQLite3 ───────────────────────────────────────────────────────────────────
find_package(SQLite3 REQUIRED)

# ── qpid-proton C++ ───────────────────────────────────────────────────────────
find_package(ProtonCpp QUIET)
if(NOT ProtonCpp_FOUND)
    pkg_check_modules(PROTON QUIET IMPORTED_TARGET qpid-proton-cpp)
    if(NOT PROTON_FOUND)
        pkg_check_modules(PROTON QUIET IMPORTED_TARGET libqpid-proton-cpp)
    endif()
    if(NOT PROTON_FOUND)
        find_library(PROTON_CPP_LIB qpid-proton-cpp)
        find_path(PROTON_INCLUDE_DIR proton/connection.hpp)
        if(PROTON_CPP_LIB AND PROTON_INCLUDE_DIR)
            add_library(PkgConfig::PROTON INTERFACE IMPORTED)
            set_target_properties(PkgConfig::PROTON PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${PROTON_INCLUDE_DIR}"
                INTERFACE_LINK_LIBRARIES      "${PROTON_CPP_LIB}")
            set(PROTON_FOUND 1)
        else()
            message(FATAL_ERROR "qpid-proton-cpp not found")
        endif()
    endif()
endif()

# ── whisper.cpp ───────────────────────────────────────────────────────────────
# Provides the C/C++ Whisper inference library (no Python required).
# Model file must be downloaded separately — see scripts/download_model.sh.
include(FetchContent)
FetchContent_Declare(whisper_cpp
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
    GIT_TAG        v1.7.5
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(whisper_cpp)

# ── Core library (shared between binary and tests) ────────────────────────────
add_library(speech_lib STATIC
    src/Config.cpp
    src/Resampler.cpp
    src/EnergyVad.cpp
    src/WhisperTranscriber.cpp
    src/TranscriptStore.cpp
)
target_include_directories(speech_lib PUBLIC include)
target_link_libraries(speech_lib PUBLIC
    whisper
    SQLite::SQLite3
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    PkgConfig::TINYXML2
    Threads::Threads
)

# ── Main binary ───────────────────────────────────────────────────────────────
add_executable(sdr_speech
    src/main.cpp
    src/AmqpListener.cpp
)
target_link_libraries(sdr_speech PRIVATE
    speech_lib
    $<IF:$<BOOL:${ProtonCpp_FOUND}>,Proton::cpp,PkgConfig::PROTON>
)
install(TARGETS sdr_speech RUNTIME DESTINATION bin)

# ── Tests ─────────────────────────────────────────────────────────────────────
include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0 GIT_SHALLOW TRUE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()

    add_executable(speech_tests
        tests/test_config.cpp
        tests/test_vad.cpp
        tests/test_resampler.cpp
        tests/test_transcript_store.cpp
    )
    target_link_libraries(speech_tests PRIVATE
        speech_lib
        GTest::gtest_main
    )
    include(GoogleTest)
    gtest_discover_tests(speech_tests)
endif()
