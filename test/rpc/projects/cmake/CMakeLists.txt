cmake_minimum_required(VERSION 3.5)
project (rpc)
set(NETP_LIB_DIR ../../../../projects/cmake)
add_subdirectory( ${NETP_LIB_DIR} ../${NETP_LIB_DIR}/build)

# Create executable file with netplus
set(server ${PROJECT_NAME}_node1)
add_executable(${server}  ../../src/rpc_node1.cpp)
TARGET_LINK_LIBRARIES(${server} PRIVATE netplus)

set(client ${PROJECT_NAME}_node2)
add_executable(${client}  ../../src/rpc_node2.cpp)
TARGET_LINK_LIBRARIES(${client} PRIVATE netplus)
