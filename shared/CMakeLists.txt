cmake_minimum_required(VERSION 3.20)
project(dashboard_shared LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Contrat commun : protocole (NDJSON), parsing .drawio, pivot loopback.
add_library(dashboard_shared STATIC
    src/protocol.cpp
    src/dashboard_file.cpp
    src/pivot.cpp
)
target_include_directories(dashboard_shared PUBLIC
    include
    third_party          # nlohmann/json (single_include)
)
if(WIN32)
    target_link_libraries(dashboard_shared PUBLIC ws2_32)
endif()

# Tests unitaires (Catch2 v3 amalgamé, D33)
enable_testing()
add_executable(shared_tests
    tests/test_main.cpp
    tests/test_protocol.cpp
    tests/test_dashboard_file.cpp
    tests/test_pivot.cpp
    tests/test_contract_golden.cpp
    third_party/catch2/catch_amalgamated.cpp
)
target_link_libraries(shared_tests PRIVATE dashboard_shared)
target_include_directories(shared_tests PRIVATE third_party/catch2)
add_test(NAME shared_tests COMMAND shared_tests)
