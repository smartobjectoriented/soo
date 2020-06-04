

add_subdirectory(asf_ta_tst)
add_subdirectory(test_crypt)

add_executable(injector injector.c)

add_executable(saveme saveme.c)
add_executable(restoreme restoreme.c)
add_executable(shutdownme shutdownme.c)

