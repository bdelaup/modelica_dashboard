cmake_minimum_required(VERSION 3.20)
project(modelica_dashboard LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Sources UTF-8 (accents dans les chaînes) : MSVC les relit en cp1252 sinon (mojibake).
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Monorepo : chaque composant a aussi son propre build (spec §7), mais la racine
# orchestre shared + modelica/csrc + tests pour les tests d'intégration.
# Le dashboard manager (Python embarqué, UI servie au navigateur — ADR-0015) n est pas buildé\n# par CMake : son packaging PyInstaller sera orchestré par scripts/ au développement.
add_subdirectory(shared)
add_subdirectory(modelica/csrc)

include(CTest)
enable_testing()
add_subdirectory(tests/integration)
