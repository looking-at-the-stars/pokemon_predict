cmake_minimum_required(VERSION 3.10)

# 设置SDK路径
set(SDK_PATH $ENV{LUCKFOX_SDK_PATH})
if(NOT SDK_PATH)
    message(WARNING "Please Set Luckfox-pico SDK Path. Such as:export LUCKFOX_SDK_PATH=/home/user/luckfox-pico")
    return()
endif()

# 设置交叉编译工具链
set(CMAKE_C_COMPILER "${SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc"
)
set(CMAKE_CXX_COMPILER "${SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-g++"
)

set(EXAMPLE_DIR "${CMAKE_CURRENT_SOURCE_DIR}") # 设定固定的源码目录
set(EXAMPLE_NAME "Pokemon_Predict") # 设定固定的项目名称

message(STATUS "Selected source directory: ${EXAMPLE_DIR}")
message(STATUS "Set project name: ${EXAMPLE_NAME}")
# 设置项目名称
project(${EXAMPLE_NAME})

# 添加库目录和第三方依赖
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ 3rdparty.out)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/utils/ utils.out)

# 设置 RPATH
set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")

# 定义编译宏
add_definitions(-DRV1106_1103)

#Opencv 4
set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/cmake/opencv4")
find_package(OpenCV REQUIRED)
#Thread POSIX 线程库
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
# 设置源代码路径
set(SRC_DIR "${EXAMPLE_DIR}/src")
file(GLOB SRC_FILES "${SRC_DIR}/*.cc")
# 生成可执行文件
add_executable(${PROJECT_NAME} ${SRC_FILES})
# 添加编译选项
add_compile_options(-g -Wall
                    -DISP_HW_V30 -DRKPLATFORM=ON -DARCH64=OFF
                    -DROCKIVA -DUAPI2
                    -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 
                    )
# 设定链接选项 让链接器在 lib/ 和 /usr/lib 目录查找共享库
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${CMAKE_CURRENT_SOURCE_DIR}/lib:/usr/lib")
# 链接库文件
target_link_libraries(${PROJECT_NAME}  
                    ${OpenCV_LIBS}
                    ${LIBRKNNRT}
                    Threads::Threads
                    rockiva
                    sample_comm
                    rockit
                    rockchip_mpp
                    rkaiq
                    pthread
                    rtsp
                    imageutils
                    fileutils
                    )
# 设置头文件包含路径
target_include_directories(${PROJECT_NAME} PRIVATE                        
                            ${OpenCV_INCLUDE_DIRS}
                            ${EXAMPLE_DIR}/include
                            ${CMAKE_CURRENT_SOURCE_DIR}
                            ${CMAKE_CURRENT_SOURCE_DIR}/utils
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/rknpu2/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/librga/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/jpeg_turbo/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/stb_image
                            ${CMAKE_CURRENT_SOURCE_DIR}/common 
                            ${CMAKE_CURRENT_SOURCE_DIR}/common/isp3.x   
                            ${CMAKE_CURRENT_SOURCE_DIR}/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rknn
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/uAPI2
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/common
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/xcore
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/algos
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/iq_parser
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/iq_parser_v2
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/smartIr
                          )
                          
# 安装规则
message(STATUS "model dir PATH: ${EXAMPLE_DIR}/model")

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install/${PROJECT_NAME}_demo")
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
SET(MODEL_DIR "${EXAMPLE_DIR}/model")
file(GLOB MODEL_FILES "${MODEL_DIR}/*")  
install(FILES ${MODEL_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/model)                    