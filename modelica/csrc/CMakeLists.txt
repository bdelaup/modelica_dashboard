cmake_minimum_required(VERSION 3.20)
project(dashboard_external LANGUAGES C)

set(CMAKE_C_STANDARD 11)

# DLL distribuée avec le package Modelica (D17/D38) : DashboardExternal.dll
add_library(DashboardExternal SHARED dashboard_client.c)
target_include_directories(DashboardExternal PUBLIC .)
set_target_properties(DashboardExternal PROPERTIES PREFIX "")
if(WIN32)
    target_link_libraries(DashboardExternal PRIVATE ws2_32)
endif()

# Installation dans le package Modelica (D17) : la DLL est copiée en post-build
# dans Resources/Library/<plateforme>, annnotation LibraryDirectory des fonctions
# externes (Dashboard/Internal/Functions/*.mo). L'en-tête est la copie dans
# Resources/Include (gardée en synchronisation avec dashboard_client.h).
if(WIN32)
    set(DASHBOARD_LIB_PLATFORM "win64")
else()
    set(DASHBOARD_LIB_PLATFORM "linux64")
endif()
add_custom_command(TARGET DashboardExternal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../Dashboard/Resources/Library/${DASHBOARD_LIB_PLATFORM}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:DashboardExternal>
        ${CMAKE_CURRENT_SOURCE_DIR}/../Dashboard/Resources/Library/${DASHBOARD_LIB_PLATFORM}/)

# Harness de test local (hors OpenModelica) : joue le rôle de la simulation.
add_executable(dashboard_harness harness.c dashboard_client.c)
target_include_directories(dashboard_harness PRIVATE .)
if(WIN32)
    target_link_libraries(dashboard_harness PRIVATE ws2_32)
endif()
