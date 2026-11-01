cmake_minimum_required(VERSION 3.20)
project(modelica_dashboard LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Monorepo : chaque composant a aussi son propre build (spec §7), mais la racine
# orchestre shared + modelica/csrc + tests pour les tests d'intégration.
add_subdirectory(shared)
add_subdirectory(modelica/csrc)

# viewer/ n'est buildé que sur Windows (WebView2) — voir viewer/CMakeLists.txt.
if(WIN32)
    add_subdirectory(viewer)
endif()

enable_testing()
add_subdirectory(tests/integration)
