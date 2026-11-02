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

# Harness de test local (hors OpenModelica) : joue le rôle de la simulation.
add_executable(dashboard_harness harness.c dashboard_client.c)
target_include_directories(dashboard_harness PRIVATE .)
if(WIN32)
    target_link_libraries(dashboard_harness PRIVATE ws2_32)
endif()
