cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(MIDISCORER_MACOS_BUNDLE_ID "org.midiscorer.app" CACHE STRING "macOS bundle identifier")
    set(MIDISCORER_MACOS_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment target")
    set(MIDISCORER_MACOS_ARCHS "" CACHE STRING "Optional CMAKE_OSX_ARCHITECTURES value (for example: arm64;x86_64)")

    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${MIDISCORER_MACOS_DEPLOYMENT_TARGET}" CACHE STRING "Minimum macOS deployment target" FORCE)
    endif()

    if(MIDISCORER_MACOS_ARCHS)
        set(CMAKE_OSX_ARCHITECTURES "${MIDISCORER_MACOS_ARCHS}" CACHE STRING "Target macOS architectures" FORCE)
    endif()
endif()

project(MidiScorer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JUCE_ROOT "" CACHE PATH "Path to JUCE root (directory containing CMakeLists.txt)")

set(_juce_source "")
if(JUCE_ROOT)
    if(NOT EXISTS "${JUCE_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR
            "JUCE_ROOT is set to '${JUCE_ROOT}' but '${JUCE_ROOT}/CMakeLists.txt' was not found.")
    endif()
    set(_juce_source "${JUCE_ROOT}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.deps/JUCE/CMakeLists.txt")
    set(_juce_source "${CMAKE_CURRENT_SOURCE_DIR}/.deps/JUCE")
elseif(WIN32 AND EXISTS "C:/JUCE/CMakeLists.txt")
    set(_juce_source "C:/JUCE")
elseif(DEFINED ENV{JUCE_ROOT} AND EXISTS "$ENV{JUCE_ROOT}/CMakeLists.txt")
    set(_juce_source "$ENV{JUCE_ROOT}")
endif()

if(_juce_source)
    message(STATUS "MidiScorer: using JUCE from ${_juce_source}")
    add_subdirectory("${_juce_source}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR
        "JUCE source tree not found.\n"
        "Clone JUCE into .deps/JUCE or configure with -DJUCE_ROOT=<path-to-JUCE>.")
endif()

if(WIN32)
    # juceaide winicon uses Drawable::createFromImageFile; square PNG with alpha works reliably.
    juce_add_gui_app(MidiScorer
        PRODUCT_NAME "MidiScorer"
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/icons/app-icon-master.png"
        ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/icons/app-icon-master.png"
    )
else()
    juce_add_gui_app(MidiScorer
        PRODUCT_NAME "MidiScorer"
    )
endif()

if(APPLE)
    set(MIDISCORER_MACOS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/src/resources/icons/MidiScorer.icns")
    if(EXISTS "${MIDISCORER_MACOS_ICON}")
        set_source_files_properties("${MIDISCORER_MACOS_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(MidiScorer PRIVATE "${MIDISCORER_MACOS_ICON}")

        set_target_properties(MidiScorer PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "${MIDISCORER_MACOS_BUNDLE_ID}"
            MACOSX_BUNDLE_BUNDLE_NAME "MidiScorer"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_ICON_FILE "MidiScorer.icns"
            XCODE_ATTRIBUTE_INFOPLIST_KEY_CFBundleIconFile "MidiScorer.icns"
        )
    else()
        message(WARNING "MidiScorer.icns not found; macOS bundle will use the default icon until src/resources/icons/MidiScorer.icns is added.")
    endif()
endif()

juce_generate_juce_header(MidiScorer)

target_sources(MidiScorer
    PRIVATE
        Main.cpp
        src/app/AppTabsHost.h
        src/app/MainComponent.h
        src/app/PlayerTabComponent.h
        src/app/PresetFileStore.h
        src/app/ScorePdfExporter.h
        src/app/ScoreRebuildService.h
        src/app/SoundsTabComponent.h
        src/app/TracksTabComponent.h
        src/app/TransportCoordinator.h
        src/app/WorkingDirectoryCopy.h
        src/midi/MidiProjectLoader.h
        src/midi/TempoMap.h
        src/midi/TrackNoteExtractor.h
        src/notation/Quantizer.h
        src/notation/ScoreModel.h
        src/notation/ScoreRenderer.h
        src/notation/SimplePdfWriter.h
        src/harmony/ChordDetector.h
        src/playback/IPlaybackPositionSource.h
        src/playback/PlaybackClock.h
        src/playback/PlaybackController.h
        src/playback/MidiFilePlaybackEngineAdapter.h
        src/playback/MidiOutputDevice.h
        src/playback/TrackMixState.h
        src/playback/TrackMixProcessor.h
        src/playback/TrackMixMidiSeed.h
        src/sounds/InstrumentCatalog.h
        src/sounds/TrackSoundProgram.h
)

target_include_directories(MidiScorer
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(MidiScorer
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(MidiScorer
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

enable_testing()

juce_add_console_app(MidiScorerTests
    PRODUCT_NAME "MidiScorerTests"
)

juce_generate_juce_header(MidiScorerTests)

target_sources(MidiScorerTests
    PRIVATE
        tests/test_main.cpp
        src/midi/TempoMap.h
        src/notation/Quantizer.h
        src/notation/ScoreModel.h
        src/notation/ScoreRenderer.h
        src/notation/SimplePdfWriter.h
        src/app/ScorePdfExporter.h
        src/app/WorkingDirectoryCopy.h
        src/harmony/ChordDetector.h
        src/playback/TrackMixState.h
        src/playback/TrackMixProcessor.h
        src/playback/TrackMixMidiSeed.h
        src/sounds/TrackSoundProgram.h
)

target_include_directories(MidiScorerTests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(MidiScorerTests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        "MIDISCORER_TEST_FIXTURES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures\""
)

target_link_libraries(MidiScorerTests
    PRIVATE
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_graphics
        juce::juce_gui_basics
)

add_test(NAME MidiScorerTests COMMAND MidiScorerTests)
set_tests_properties(MidiScorerTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
