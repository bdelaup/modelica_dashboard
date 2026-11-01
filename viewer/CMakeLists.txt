cmake_minimum_required(VERSION 3.20)
# viewer.exe — Windows uniquement en V1 (D1) : C++/Win32 + WebView2 (D16).
# Buildé uniquement sur Windows (racine : if(WIN32)). Les ressources web embarquées
# sont dans src/web ; draw.io statique dans third_party/drawio (D34).
project(dashboard_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_CURRENT_BINARY_DIR}/shared)

find_package(WebView2 REQUIRED) # NuGet Microsoft.Web.WebView2, via vcpkg ou package CI

# Ressources embarquées (IHM app:// + draw.io) compilées dans l'exe (D9)
file(GLOB_RECURSE WEB_ASSETS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/web/*.js
    ${CMAKE_CURRENT_SOURCE_DIR}/src/web/*.html
    ${CMAKE_CURRENT_SOURCE_DIR}/src/web/*.css
    ${CMAKE_CURRENT_SOURCE_DIR}/src/web/i18n/*
)

add_executable(viewer WIN32
    src/cpp/main.cpp
    src/cpp/webview_host.cpp
    src/cpp/webview_host.hpp
    src/cpp/resources.rc
)
target_link_libraries(viewer PRIVATE dashboard_shared WebView2::WebView2)
target_include_directories(viewer PRIVATE src/cpp)
# Génère resources.h avec les assets embarqués (généré au build par cmake)
target_compile_definitions(viewer PRIVATE DASHBOARD_EMBED_WEB=1)
