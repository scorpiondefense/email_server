# POP3 Tests CMakeLists.txt

add_executable(test_pop3
    test_pop3.cpp
    ../src/pop3_commands.cpp
    ../src/pop3_session.cpp
)

target_include_directories(test_pop3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(test_pop3 PRIVATE
    Catch2::Catch2WithMain
    email_common
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${SQLite3_LIBRARIES}
    Threads::Threads
)

include(CTest)
include(Catch)
catch_discover_tests(test_pop3)
