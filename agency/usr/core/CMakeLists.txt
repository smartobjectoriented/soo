
add_executable(agency core.c leds.c)

add_subdirectory(migration)
add_subdirectory(upgrader)
add_subdirectory(asf)
add_subdirectory(device_access)
add_subdirectory(injector)

target_link_libraries(agency dcm frozen pthread z expat archive lz4)
