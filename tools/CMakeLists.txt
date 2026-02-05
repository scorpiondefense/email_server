# Tools CMakeLists.txt

add_executable(create_user create_user.cpp)

target_include_directories(create_user PRIVATE
    ${CMAKE_SOURCE_DIR}/common/include
    ${SQLite3_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(create_user PRIVATE
    email_common
    ${SQLite3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
)

install(TARGETS create_user
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install scripts
install(PROGRAMS generate_certs.sh
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
