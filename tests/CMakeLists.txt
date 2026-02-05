# Tests CMakeLists.txt

# Include individual protocol tests
add_subdirectory(${CMAKE_SOURCE_DIR}/pop3/tests pop3_tests)
add_subdirectory(${CMAKE_SOURCE_DIR}/imap/tests imap_tests)
add_subdirectory(${CMAKE_SOURCE_DIR}/smtp/tests smtp_tests)

# Integration tests
add_executable(test_integration
    integration/test_full_flow.cpp
)

target_include_directories(test_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/common/include
    ${CMAKE_SOURCE_DIR}/pop3/include
    ${CMAKE_SOURCE_DIR}/imap/include
    ${CMAKE_SOURCE_DIR}/smtp/include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(test_integration PRIVATE
    Catch2::Catch2WithMain
    email_common
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${SQLite3_LIBRARIES}
    Threads::Threads
)

include(CTest)
include(Catch)
catch_discover_tests(test_integration)
