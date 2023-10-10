project(mqtt-client LANGUAGES C CXX)

file(GLOB SRC *.cpp)

add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} paho-mqtt3as paho-mqttpp3 jsoncpp)
