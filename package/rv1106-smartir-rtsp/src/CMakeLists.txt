# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

project(rv1106-smartir-rtsp LANGUAGES C)

add_executable(rv1106-smartir-rtsp main.c)

if(DEFINED WIDTH AND DEFINED HEIGHT)
    target_compile_definitions(rv1106-smartir-rtsp PRIVATE WIDTH=${WIDTH} HEIGHT=${HEIGHT})
endif()

find_library(
  ROCKCHIP_MPP
  NAMES rockchip_mpp
  PATHS ${RV1106_IPC_SDK_DIR}/media/mpp/release_mpp_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib
  REQUIRED
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
find_library(
  ROCKIT
  NAMES rockit
  PATHS ${RV1106_IPC_SDK_DIR}/media/rockit/rockit/lib/lib32/
  REQUIRED
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)

set(RGA_DIR
    ${RV1106_IPC_SDK_DIR}/media/rga/release_rga_rv1106_arm-rockchip830-linux-uclibcgnueabihf
)
find_library(
  RGA
  NAMES rga
  PATHS ${RGA_DIR}/lib
  REQUIRED
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
find_library(
  RTSP
  NAMES rtsp
  PATHS ${RV1106_IPC_SDK_DIR}/media/common_algorithm/common_algorithm/misc/lib/arm-rockchip830-linux-uclibcgnueabihf
  REQUIRED
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)

set(RKAIQ_DIR
    ${RV1106_IPC_SDK_DIR}/media/isp/release_camera_engine_rkaiq_rv1106_arm-rockchip830-linux-uclibcgnueabihf
)
find_library(
  RKAIQ
  NAMES rkaiq
  PATHS ${RKAIQ_DIR}/lib
  REQUIRED
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
find_library(
  SMARTIR
  NAMES smartIr
  PATHS ${RKAIQ_DIR}/lib
  REQUIRED
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)

target_include_directories(
  rv1106-smartir-rtsp
  PRIVATE
  ${RV1106_IPC_SDK_DIR}/media/common_algorithm/common_algorithm/misc/include
  ${RV1106_IPC_SDK_DIR}/media/rockit/rockit/mpi/sdk/include
  ${RGA_DIR}/include
  ${RGA_DIR}/include/rga
  ${RKAIQ_DIR}/include
  ${RKAIQ_DIR}/include/rkaiq
  ${RKAIQ_DIR}/include/rkaiq/algos
  ${RKAIQ_DIR}/include/rkaiq/common
  ${RKAIQ_DIR}/include/rkaiq/iq_parser
  ${RKAIQ_DIR}/include/rkaiq/iq_parser_v2
  ${RKAIQ_DIR}/include/rkaiq/smartIr
  ${RKAIQ_DIR}/include/rkaiq/uAPI2
  ${RKAIQ_DIR}/include/rkaiq/xcore
)

target_link_libraries(
  rv1106-smartir-rtsp
  PRIVATE
  ${ROCKCHIP_MPP}
  ${ROCKIT}
  ${RGA}
  ${RTSP}
  ${RKAIQ}
  ${SMARTIR}
  pthread
)

install(TARGETS rv1106-smartir-rtsp RUNTIME DESTINATION bin)
