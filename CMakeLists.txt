cmake_minimum_required(VERSION 3.20)

project(MailForge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MAILFORGE_BUILD_TESTS "Build MailForge tests" ON)

find_package(Boost 1.74 REQUIRED)
find_package(Threads REQUIRED)

add_library(mailforge_core
    src/app/Application.cpp
    src/config/ConfigManager.cpp
    src/logging/Logger.cpp
    src/mime/MailMessage.cpp
    src/network/ClientSession.cpp
    src/network/TcpListener.cpp
    src/smtp/SMTPParser.cpp
    src/smtp/SMTPStateMachine.cpp
    src/storage/MailStorage.cpp
)

target_include_directories(mailforge_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_options(mailforge_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(mailforge_core
    PUBLIC
        Boost::headers
        Threads::Threads
)

add_executable(mailforge src/main.cpp)
target_link_libraries(mailforge PRIVATE mailforge_core)

if(MAILFORGE_BUILD_TESTS)
    enable_testing()
    add_executable(mailforge_smoke_tests tests/smoke_tests.cpp)
    target_link_libraries(mailforge_smoke_tests PRIVATE mailforge_core)
    add_test(NAME mailforge_smoke_tests COMMAND mailforge_smoke_tests)
    set_tests_properties(mailforge_smoke_tests PROPERTIES
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )

    add_executable(mailforge_integration_tests tests/integration_tests.cpp)
    target_link_libraries(mailforge_integration_tests PRIVATE mailforge_core)
    add_test(NAME mailforge_integration_tests COMMAND mailforge_integration_tests)
    set_tests_properties(mailforge_integration_tests PROPERTIES
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endif()
