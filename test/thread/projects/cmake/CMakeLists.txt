cmake_minimum_required(VERSION 3.5)
project (thread)
set(NETP_LIB_DIR ../../../../projects/cmake)
add_subdirectory( ${NETP_LIB_DIR} ../${NETP_LIB_DIR}/build)

# Create executable file with netplus
add_executable(${PROJECT_NAME}  ../../src/main.cpp)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE netplus)