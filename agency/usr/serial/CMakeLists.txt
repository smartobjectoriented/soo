project(lr4_test LANGUAGES C)

file(GLOB SRC *.c)

add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} -lpthread)