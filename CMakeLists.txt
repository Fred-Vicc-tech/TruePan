cmake_minimum_required(VERSION 3.15)
project(TruePan CXX)

set(CMAKE_CXX_STANDARD 14)
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
endif()

# --- Pluginet (bara på Windows) -------------------------------------------
if(WIN32)
    add_library(TruePan MODULE TruePan.cpp TruePanDSP.h Manual.h vst2.h)
    set_target_properties(TruePan PROPERTIES PREFIX "" SUFFIX ".dll")
    target_link_libraries(TruePan PRIVATE gdiplus user32 gdi32 comdlg32 shell32)
endif()

# --- Automatiska tester ----------------------------------------------------
enable_testing()

add_executable(TruePanTests TruePanTests.cpp)
add_test(NAME dsp COMMAND TruePanTests)

add_executable(HostTests HostTests.cpp TruePan.cpp)
if(WIN32)
    target_link_libraries(HostTests PRIVATE gdiplus user32 gdi32 comdlg32 shell32)
endif()
add_test(NAME host COMMAND HostTests)
