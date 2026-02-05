# SMTP Tests CMakeLists.txt

add_executable(test_smtp
    test_smtp.cpp
    ../src/smtp_commands.cpp
    ../src/smtp_session.cpp
    ../src/smtp_relay.cpp
)

target_include_directories(test_smtp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(test_smtp PRIVATE
    Catch2::Catch2WithMain
    email_common
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${SQLite3_LIBRARIES}
    Threads::Threads
    resolv
)

include(CTest)
include(Catch)
catch_discover_tests(test_smtp)
