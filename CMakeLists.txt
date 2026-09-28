cmake_minimum_required(VERSION 3.20)

# Keep in sync with kVersionLabel in src/theme.h.
project(WLQuickGen VERSION 1.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static runtime: one portable .exe, no VC++ redistributable needed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WLQG_ARCH x64)
else()
    set(WLQG_ARCH x86)
endif()

add_executable(WLQuickGen WIN32
    src/WLQuickGen.cpp
    src/i18n.cpp
    src/theme.cpp
    WLQuickGen.rc
)
target_include_directories(WLQuickGen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WLQuickGen PRIVATE UNICODE _UNICODE)
target_link_libraries(WLQuickGen PRIVATE comctl32 gdi32 shell32 user32)
set_target_properties(WLQuickGen PROPERTIES
    OUTPUT_NAME "WLQuickGen-${WLQG_ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MSVC)
    target_compile_options(WLQuickGen PRIVATE /W4 /permissive- /utf-8)
    # The executable carries its own manifest through WLQuickGen.rc.
    target_link_options(WLQuickGen PRIVATE /MANIFEST:NO)
elseif(MINGW)
    target_compile_options(WLQuickGen PRIVATE -Wall -Wextra)
    target_link_options(WLQuickGen PRIVATE -municode -static)
endif()
