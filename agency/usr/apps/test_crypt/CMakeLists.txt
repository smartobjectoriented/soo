add_executable(test_crypt test_crypt.c)

target_link_libraries(test_crypt crypto ssl tomcrypt)
