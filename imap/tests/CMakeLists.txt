# IMAP Tests CMakeLists.txt

add_executable(test_imap
    test_imap.cpp
    ../src/imap_parser.cpp
    ../src/imap_commands.cpp
    ../src/imap_session.cpp
)

target_include_directories(test_imap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(test_imap PRIVATE
    Catch2::Catch2WithMain
    email_common
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${SQLite3_LIBRARIES}
    Threads::Threads
)

include(CTest)
include(Catch)
catch_discover_tests(test_imap)
