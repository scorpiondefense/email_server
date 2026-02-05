# POP3 Server CMakeLists.txt

set(POP3_SOURCES
    src/pop3_server.cpp
    src/pop3_session.cpp
    src/pop3_commands.cpp
    src/main.cpp
)

set(POP3_HEADERS
    include/pop3_server.hpp
    include/pop3_session.hpp
    include/pop3_commands.hpp
)

add_executable(pop3_server ${POP3_SOURCES} ${POP3_HEADERS})

target_include_directories(pop3_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(pop3_server PRIVATE
    email_common
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${SQLite3_LIBRARIES}
    Threads::Threads
)

install(TARGETS pop3_server
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Tests
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
