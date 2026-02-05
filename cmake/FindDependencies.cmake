# FindDependencies.cmake
# Discovers and configures all required dependencies for the email server suite

function(find_dependencies)
    # Boost (Asio for async I/O)
    find_package(Boost 1.74 REQUIRED COMPONENTS system)
    if(Boost_FOUND)
        message(STATUS "Found Boost ${Boost_VERSION}")
        message(STATUS "  Include: ${Boost_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${Boost_LIBRARIES}")
    else()
        message(FATAL_ERROR "Boost >= 1.74 is required. Install with: apt install libboost-all-dev")
    endif()

    # OpenSSL (TLS/SSL support)
    if(ENABLE_TLS)
        find_package(OpenSSL 1.1.1 REQUIRED)
        if(OPENSSL_FOUND)
            message(STATUS "Found OpenSSL ${OPENSSL_VERSION}")
            message(STATUS "  Include: ${OPENSSL_INCLUDE_DIR}")
            message(STATUS "  Libraries: ${OPENSSL_LIBRARIES}")
            add_compile_definitions(ENABLE_TLS=1)
        else()
            message(FATAL_ERROR "OpenSSL >= 1.1.1 is required. Install with: apt install libssl-dev")
        endif()
    endif()

    # SQLite3 (Authentication database)
    find_package(SQLite3 3.35 REQUIRED)
    if(SQLite3_FOUND)
        message(STATUS "Found SQLite3 ${SQLite3_VERSION}")
        message(STATUS "  Include: ${SQLite3_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${SQLite3_LIBRARIES}")
    else()
        message(FATAL_ERROR "SQLite3 >= 3.35 is required. Install with: apt install libsqlite3-dev")
    endif()

    # Catch2 (Testing framework)
    if(BUILD_TESTS)
        find_package(Catch2 3 QUIET)
        if(Catch2_FOUND)
            message(STATUS "Found Catch2 ${Catch2_VERSION}")
        else()
            message(STATUS "Catch2 not found - fetching from GitHub")
            include(FetchContent)
            FetchContent_Declare(
                Catch2
                GIT_REPOSITORY https://github.com/catchorg/Catch2.git
                GIT_TAG v3.4.0
            )
            FetchContent_MakeAvailable(Catch2)
            list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
        endif()
    endif()

    # Threads
    find_package(Threads REQUIRED)

    # Make variables available in parent scope
    set(Boost_INCLUDE_DIRS ${Boost_INCLUDE_DIRS} PARENT_SCOPE)
    set(Boost_LIBRARIES ${Boost_LIBRARIES} PARENT_SCOPE)
    set(OPENSSL_INCLUDE_DIR ${OPENSSL_INCLUDE_DIR} PARENT_SCOPE)
    set(OPENSSL_LIBRARIES ${OPENSSL_LIBRARIES} PARENT_SCOPE)
    set(SQLite3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIRS} PARENT_SCOPE)
    set(SQLite3_LIBRARIES ${SQLite3_LIBRARIES} PARENT_SCOPE)
endfunction()

# Helper function to create a server executable
function(add_server_executable name)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES HEADERS LIBS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})

    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
        ${SQLite3_INCLUDE_DIRS}
    )

    target_link_libraries(${name} PRIVATE
        email_common
        ${Boost_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${SQLite3_LIBRARIES}
        Threads::Threads
        ${ARG_LIBS}
    )

    install(TARGETS ${name}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endfunction()

# Helper function to create a test executable
function(add_test_executable name)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs SOURCES LIBS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})

    target_link_libraries(${name} PRIVATE
        Catch2::Catch2WithMain
        email_common
        ${ARG_LIBS}
    )

    include(CTest)
    include(Catch)
    catch_discover_tests(${name})
endfunction()
