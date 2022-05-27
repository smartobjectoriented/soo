
add_executable(agency 
        core.c 
        asf.c ta_installation.c
        device_access.c 
        injector.c 
        leds.c
        migration.c 
        upgrader.c
)

target_link_libraries(agency dcm frozen pthread z expat archive lz4)
