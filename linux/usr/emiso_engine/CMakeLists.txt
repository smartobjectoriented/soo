project(emiso_engine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} emiso_engine.cpp
                               utils.cpp
                               webserver/webserver.cpp
                               webserver/system/system.cpp
                               webserver/container/container.cpp
                               webserver/image/image.cpp
                               webserver/network/network.cpp
                               webserver/volume/volume.cpp
                               daemon/image.cpp
                               cli/cli.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE include)

target_link_libraries(${PROJECT_NAME} httpserver jsoncpp readline)

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_BINARY_DIR}/bin)
