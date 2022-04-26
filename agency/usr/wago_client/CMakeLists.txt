project(wago-client LANGUAGES C CXX)

file(GLOB SRC *.cpp)

add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} curl)