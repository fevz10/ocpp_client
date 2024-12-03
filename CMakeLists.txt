project(ocpp_client LANGUAGES C)
cmake_minimum_required(VERSION 3.5)

find_package(libwebsockets CONFIG REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)
file(GLOB SOURCES "src/*.c" main.c)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE websockets)
