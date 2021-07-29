

add_subdirectory(asf_ta_tst)

add_executable(injector injector.c)

add_executable(saveme saveme.c)
add_executable(restoreme restoreme.c)
add_executable(shutdownme shutdownme.c)

add_executable(melist melist.c)

target_link_libraries(saveme zip)
target_link_libraries(restoreme zip)

add_executable(bt_test bt_test.c)
target_link_libraries(bt_test pthread)
