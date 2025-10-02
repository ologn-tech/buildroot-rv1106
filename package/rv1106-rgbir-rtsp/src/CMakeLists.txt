# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

project(rv1106-rgbir-rtsp LANGUAGES C CXX)

add_executable(rv1106-rgbir-rtsp main.cc)

# Add NEON optimization flags for Cortex-A7
target_compile_options(rv1106-rgbir-rtsp PRIVATE
    -mcpu=cortex-a7
    -mfpu=neon-vfpv4
    -mfloat-abi=hard
    -O3
    -ffast-math
    -ftree-vectorize
    -funsafe-math-optimizations
)

add_subdirectory(${LIBMPIX_DIR}/ports/posix libmpix)
libmpix_init(rv1106-rgbir-rtsp)

find_package(OpenCV REQUIRED)
find_library(
  ROCKCHIP_MPP
  NAMES
  rockchip_mpp
  PATHS
  ${RV1106_IPC_SDK_DIR}/media/mpp/release_mpp_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib
  REQUIRED
  NO_CMAKE_FIND_ROOT_PATH
)
find_library(
  ROCKIT
  NAMES
  rockit
  PATHS
  ${RV1106_IPC_SDK_DIR}/media/rockit/rockit/lib/lib32/
  REQUIRED
  NO_CMAKE_FIND_ROOT_PATH
)
find_library(
  RGA
  NAMES
  rga
  PATHS
  ${RV1106_IPC_SDK_DIR}/media/rga/release_rga_rv1106_arm-rockchip830-linux-uclibcgnueabihf/lib
  REQUIRED
  NO_CMAKE_FIND_ROOT_PATH
)
find_library(
  RTSP
  NAMES
  rtsp
  PATHS
  ${RV1106_IPC_SDK_DIR}/media/common_algorithm/common_algorithm/misc/lib/arm-rockchip830-linux-uclibcgnueabihf
  REQUIRED
  NO_CMAKE_FIND_ROOT_PATH
)

target_include_directories(
  rv1106-rgbir-rtsp
  PRIVATE
  ${OpenCV_INCLUDE_DIRS}
	${RV1106_IPC_SDK_DIR}/media/common_algorithm/common_algorithm/misc/include
  ${RV1106_IPC_SDK_DIR}/media/rockit/rockit/mpi/sdk/include
)

target_link_libraries(
  rv1106-rgbir-rtsp
  PUBLIC
  libmpix
  ${OpenCV_LIBS}
  ${ROCKCHIP_MPP}
  ${ROCKIT}
  ${RGA}
  ${RTSP}
)

install(TARGETS rv1106-rgbir-rtsp
        RUNTIME DESTINATION bin
)
