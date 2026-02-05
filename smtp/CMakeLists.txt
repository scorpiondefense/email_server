# SMTP Server CMakeLists.txt

set(SMTP_SOURCES
    src/smtp_server.cpp
    src/smtp_session.cpp
    src/smtp_commands.cpp
    src/smtp_relay.cpp
    src/main.cpp
)

set(SMTP_HEADERS
    include/smtp_server.hpp
    include/smtp_session.hpp
    include/smtp_commands.hpp
    include/smtp_relay.hpp
)

add_executable(smtp_server ${SMTP_SOURCES} ${SMTP_HEADERS})

target_include_directories(smtp_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(smtp_server PRIVATE
    email_common
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${SQLite3_LIBRARIES}
    Threads::Threads
)

install(TARGETS smtp_server
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Tests
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
