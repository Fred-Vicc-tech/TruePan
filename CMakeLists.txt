cmake_minimum_required(VERSION 3.25)
project(TruePan VERSION 2.6.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statisk runtime: inga extra DLL-filer behövs på användarens dator
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# Spärr: MSVC 14.50 (Visual Studio 2026) och nyare kan inte bygga för Windows 7.
if(MSVC AND MSVC_VERSION GREATER_EQUAL 1950)
    message(FATAL_ERROR "Denna kompilator stöder inte Windows 7. Använd Visual Studio 2022 (toolset v143).")
endif()

if(MSVC)
    # Källfilerna är UTF-8 (svenska kommentarer)
    add_compile_options(/utf-8)
endif()

if(WIN32)
    # Kompatibilitet med Windows 7
    add_compile_definitions(_WIN32_WINNT=0x0601 WINVER=0x0601 NOMINMAX _CRT_SECURE_NO_WARNINGS)
    set(TRUEPAN_EDITOR_SOURCES TruePanEditor.cpp Manual.h)
    set(TRUEPAN_EDITOR_LIBS gdiplus user32 gdi32 comdlg32 shell32)
endif()

# --- VST2-pluginet (bara på Windows) ---------------------------------------
if(WIN32)
    add_library(TruePan MODULE TruePan.cpp TruePanCore.h TruePanDSP.h vst2.h ${TRUEPAN_EDITOR_SOURCES})
    set_target_properties(TruePan PROPERTIES PREFIX "" SUFFIX ".dll")
    target_link_libraries(TruePan PRIVATE ${TRUEPAN_EDITOR_LIBS})
endif()

# --- Automatiska tester ----------------------------------------------------
enable_testing()

add_executable(TruePanTests TruePanTests.cpp)
add_test(NAME dsp COMMAND TruePanTests)

add_executable(HostTests HostTests.cpp TruePan.cpp ${TRUEPAN_EDITOR_SOURCES})
target_link_libraries(HostTests PRIVATE ${TRUEPAN_EDITOR_LIBS})
add_test(NAME host COMMAND HostTests)

# Bilder av gränssnittet (Windows): ritas med samma kod som pluginet
if(WIN32)
    add_executable(RenderGui RenderGui.cpp TruePanCore.h ${TRUEPAN_EDITOR_SOURCES})
    target_link_libraries(RenderGui PRIVATE ${TRUEPAN_EDITOR_LIBS})
endif()

# --- VST3-pluginet (om Steinbergs VST 3 SDK finns, MIT-licens) --------------
# Bygget hämtar SDK:t (se .github/workflows/build.yml) och anger sökvägen:
#   cmake -DTRUEPAN_VST3_SDK=<sökväg till vst3sdk> ...
set(TRUEPAN_VST3_SDK "" CACHE PATH "Sökväg till Steinbergs VST 3 SDK")
if(TRUEPAN_VST3_SDK)
    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
    set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
    set(SMTG_USE_STATIC_CRT ON CACHE BOOL "" FORCE)
    set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)     # Steinbergs validator körs efter varje bygge
    add_subdirectory(${TRUEPAN_VST3_SDK} ${CMAKE_BINARY_DIR}/vst3sdk)
    set(SDK_ROOT ${TRUEPAN_VST3_SDK})
    set(public_sdk_SOURCE_DIR ${TRUEPAN_VST3_SDK}/public.sdk)   # behövs för pluginets ingångspunkt

    smtg_add_vst3plugin(TruePanVST3
        PACKAGE_NAME "TruePan"
        SOURCES_LIST
            TruePanVST3.cpp TruePanCore.h TruePanDSP.h
            ${TRUEPAN_VST3_SDK}/public.sdk/source/vst/vstsinglecomponenteffect.cpp
            ${TRUEPAN_VST3_SDK}/public.sdk/source/vst/vstsinglecomponenteffect.h
            ${TRUEPAN_EDITOR_SOURCES})
    target_link_libraries(TruePanVST3 PRIVATE sdk ${TRUEPAN_EDITOR_LIBS})
    target_include_directories(TruePanVST3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
