cmake_minimum_required(VERSION 3.20)
# viewer — deux shells au-dessus de viewer_core (logique commune, sans WebView2) :
#   - Windows V1 (D1/D16) : viewer.exe Win32 + WebView2 (src/cpp)
#   - Partout (Linux/macOS/CI, portage tests) : viewer_headless, shell stdio NDJSON (src/headless)
project(dashboard_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest) # définit BUILD_TESTING=ON par défaut

if(NOT TARGET dashboard_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_CURRENT_BINARY_DIR}/shared)
endif()

# --- viewer_core : logique commune, testable sans WebView2 ---
add_library(viewer_core STATIC
    src/core/viewer_core.cpp
)
target_include_directories(viewer_core PUBLIC src/core)
target_link_libraries(viewer_core PUBLIC dashboard_shared)

# --- Shell headless (portable) ---
add_executable(viewer_headless src/headless/main.cpp)
target_link_libraries(viewer_headless PRIVATE viewer_core)

# --- Tests du core (Catch2, CTest) ---
if(BUILD_TESTING)
    add_executable(viewer_core_tests
        tests/test_viewer_core.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../shared/third_party/catch2/catch_amalgamated.cpp
    )
    target_link_libraries(viewer_core_tests PRIVATE viewer_core)
    target_include_directories(viewer_core_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../shared/third_party/catch2)
    add_test(NAME viewer_core_tests COMMAND viewer_core_tests)
endif()

# --- Shell Win32/WebView2 : Windows uniquement (D1) ---
# Le SDK vient du NuGet Microsoft.Web.Web.WebView2 (pas de config CMake : lien direct
# headers + WebView2LoaderStatic.lib). Installer via scripts/build.ps1 ou :
#   nuget install Microsoft.Web.WebView2 -OutputDirectory .nuget
# puis -DWebView2_DIR=$PWD/.nuget/Microsoft.Web.WebView2  (défaut : .nuget/...)
if(WIN32)
    if(NOT DEFINED WebView2_DIR)
        set(WebView2_DIR "${CMAKE_SOURCE_DIR}/.nuget/Microsoft.Web.WebView2")
    endif()
    set(WV2_INCLUDE "${WebView2_DIR}/build/native/include")
    set(WV2_LIBDIR "${WebView2_DIR}/build/native/${CMAKE_GENERATOR_PLATFORM}")
    if(NOT EXISTS "${WV2_INCLUDE}/WebView2.h")
        message(FATAL_ERROR
            "SDK WebView2 introuvable (${WV2_INCLUDE}/WebView2.h). Installer le NuGet :\n"
            "  nuget install Microsoft.Web.WebView2 -OutputDirectory .nuget\n"
            "  cmake -S . -B build -DWebView2_DIR=$PWD/.nuget/Microsoft.Web.WebView2")
    endif()
    add_executable(viewer WIN32
        src/cpp/main.cpp
        src/cpp/webview_host.cpp
        src/cpp/webview_host.hpp
    )
    target_link_libraries(viewer PRIVATE viewer_core)
    target_include_directories(viewer PRIVATE src/cpp "${WV2_INCLUDE}")
    target_link_directories(viewer PRIVATE "${WV2_LIBDIR}")
    target_link_libraries(viewer PRIVATE WebView2LoaderStatic.lib) # lien statique : exe autonome (L2)
    # WIL (Windows Implementation Library) pour les callbacks WebView2
    find_package(Microsoft.Windows.ImplementationLibrary QUIET)
    if(NOT Microsoft.Windows.ImplementationLibrary_FOUND)
        # fallback : WIL header-only depuis le NuGet Microsoft.Windows.ImplementationLibrary
        set(WIL_DIR "${CMAKE_SOURCE_DIR}/.nuget/Microsoft.Windows.ImplementationLibrary/build/native/include")
        if(EXISTS "${WIL_DIR}/wil/com.h")
            target_include_directories(viewer PRIVATE "${WIL_DIR}")
        endif()
    endif()
endif()
