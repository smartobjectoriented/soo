

add_subdirectory(asf_ta_tst)

add_executable(injector injector.c)

add_executable(saveme saveme.c)
add_executable(restoreme restoreme.c)
add_executable(shutdownme shutdownme.c)
add_executable(blacklist_soo blacklist_soo.c)

add_executable(melist melist.c)

target_link_libraries(saveme zip)
target_link_libraries(restoreme zip)
