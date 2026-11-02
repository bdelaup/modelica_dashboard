cmake_minimum_required(VERSION 3.20)
project(dashboard_integration LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



add_executable(test_end_to_end test_end_to_end.cpp)
target_link_libraries(test_end_to_end PRIVATE dashboard_shared)

enable_testing()
# Le harness C (modelica/csrc) doit être buildé d'abord (build.ps1 s'en charge).
if(WIN32)
    add_test(NAME end_to_end
        COMMAND ${CMAKE_COMMAND} -E env "DASHBOARD_HARNESS=$<TARGET_FILE:dashboard_harness>"
                $<TARGET_FILE:test_end_to_end>)
else()
    add_test(NAME end_to_end COMMAND test_end_to_end $<TARGET_FILE:dashboard_harness>)
endif()

add_executable(bench_framing bench_framing.cpp)
target_link_libraries(bench_framing PRIVATE dashboard_shared)
add_test(NAME bench_framing COMMAND bench_framing 3 1000 100)
set_tests_properties(bench_framing PROPERTIES TIMEOUT 60)

# E2E headless : viewer_headless en process réel + client sim Python
find_program(PYTHON3 NAMES python3 python)
if(PYTHON3)
    add_test(NAME headless_e2e
        COMMAND ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/test_headless_e2e.py $<TARGET_FILE:viewer_headless>)
    set_tests_properties(headless_e2e PROPERTIES TIMEOUT 60)
endif()
