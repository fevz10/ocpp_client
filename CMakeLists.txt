project(ocpp_client C)
cmake_minimum_required(VERSION 3.5)

find_package(libwebsockets CONFIG REQUIRED)
set(SRCS main.c cJSON.c)

add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME} PRIVATE websockets)
