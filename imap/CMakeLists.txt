# IMAP Server CMakeLists.txt

set(IMAP_SOURCES
    src/imap_server.cpp
    src/imap_session.cpp
    src/imap_commands.cpp
    src/imap_parser.cpp
    src/main.cpp
)

set(IMAP_HEADERS
    include/imap_server.hpp
    include/imap_session.hpp
    include/imap_commands.hpp
    include/imap_parser.hpp
)

add_executable(imap_server ${IMAP_SOURCES} ${IMAP_HEADERS})

target_include_directories(imap_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(imap_server PRIVATE
    email_common
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${SQLite3_LIBRARIES}
    Threads::Threads
)

install(TARGETS imap_server
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Tests
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
