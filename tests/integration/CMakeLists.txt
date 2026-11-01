cmake_minimum_required(VERSION 3.20)
project(dashboard_integration LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



add_executable(test_end_to_end test_end_to_end.cpp)
target_link_libraries(test_end_to_end PRIVATE dashboard_shared)

enable_testing()
# Le harness C (modelica/csrc) doit être buildé d'abord (build.ps1 s'en charge).
add_test(NAME end_to_end COMMAND test_end_to_end $<TARGET_FILE:dashboard_harness>)
