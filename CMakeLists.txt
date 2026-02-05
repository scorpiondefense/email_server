cmake_minimum_required(VERSION 3.20)

project(email_server
    VERSION 1.0.0
    DESCRIPTION "Complete Email Server Suite with POP3, IMAP, and SMTP"
    LANGUAGES CXX
)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include custom cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(FindDependencies)

# Options
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_TOOLS "Build utility tools" ON)
option(ENABLE_TLS "Enable TLS/SSL support" ON)

# Find dependencies
find_dependencies()

# Common library
add_subdirectory(common)

# Protocol servers
add_subdirectory(pop3)
add_subdirectory(imap)
add_subdirectory(smtp)

# Tools
if(BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

install(DIRECTORY config/
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/email_server
    FILES_MATCHING PATTERN "*.example"
)

# Package configuration
set(CPACK_PACKAGE_NAME "email-server")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "Private Email Server Project")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "admin@example.com")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libssl1.1, libsqlite3-0, libboost-system1.74.0")
include(CPack)

# Summary
message(STATUS "")
message(STATUS "Email Server Suite Configuration:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  TLS Support:  ${ENABLE_TLS}")
message(STATUS "  Build Tests:  ${BUILD_TESTS}")
message(STATUS "  Build Tools:  ${BUILD_TOOLS}")
message(STATUS "")
