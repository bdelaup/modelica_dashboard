cmake_minimum_required(VERSION 3.20)
project(dashboard_integration LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# E2E sim <-> dashboard manager : harness C (modelica/csrc) + pivot reel (shared),
# pilote depuis le test C++ (commandes cmd set, flux data/state).
add_executable(test_end_to_end test_end_to_end.cpp ${CMAKE_SOURCE_DIR}/modelica/csrc/harness.c ${CMAKE_SOURCE_DIR}/modelica/csrc/dashboard_client.c)
target_compile_definitions(test_end_to_end PRIVATE DASHBOARD_HARNESS_NO_MAIN)
target_link_libraries(test_end_to_end PRIVATE dashboard_shared)
target_include_directories(test_end_to_end PRIVATE ${CMAKE_SOURCE_DIR}/modelica/csrc)

enable_testing()
add_test(NAME end_to_end COMMAND test_end_to_end)

add_executable(bench_framing bench_framing.cpp)
target_link_libraries(bench_framing PRIVATE dashboard_shared)
add_test(NAME bench_framing COMMAND bench_framing 3 1000 100)
set_tests_properties(bench_framing PROPERTIES TIMEOUT 60)
