cmake_minimum_required(VERSION 3.5)
project (cs)
set(NETP_LIB_DIR ../../../../projects/cmake)
add_subdirectory( ${NETP_LIB_DIR} ../${NETP_LIB_DIR}/build)

# Create executable file with netplus
set(server ${PROJECT_NAME}_server)
add_executable(${server}  ../../src/server.cpp)
TARGET_LINK_LIBRARIES(${server} PRIVATE netplus)

set(client ${PROJECT_NAME}_client)
add_executable(${client}  ../../src/client.cpp)
TARGET_LINK_LIBRARIES(${client} PRIVATE netplus)
