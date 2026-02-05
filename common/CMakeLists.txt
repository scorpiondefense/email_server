# Common library CMakeLists.txt

set(COMMON_SOURCES
    src/config.cpp
    src/logger.cpp
    src/ssl_context.cpp
    src/auth/authenticator.cpp
    src/storage/maildir.cpp
    src/net/session.cpp
    src/net/server.cpp
)

set(COMMON_HEADERS
    include/config.hpp
    include/logger.hpp
    include/ssl_context.hpp
    include/auth/authenticator.hpp
    include/storage/maildir.hpp
    include/net/session.hpp
    include/net/server.hpp
)

add_library(email_common STATIC ${COMMON_SOURCES} ${COMMON_HEADERS})

target_include_directories(email_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(email_common PUBLIC
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${SQLite3_LIBRARIES}
    Threads::Threads
)

# Compile definitions
target_compile_definitions(email_common PUBLIC
    BOOST_ASIO_NO_DEPRECATED
    $<$<BOOL:${ENABLE_TLS}>:ENABLE_TLS=1>
)

# Install headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/email_server
    FILES_MATCHING PATTERN "*.hpp"
)

install(TARGETS email_common
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
