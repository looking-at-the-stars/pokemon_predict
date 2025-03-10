#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include "mobilenet.h"
#include "file_utils.h"
#include "luckfox_mpi.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#define IMAGENET_CLASSES_FILE "./model/synset.txt"
#define MODEL_FILE "./model/mobilenet.rknn"

#define DISP_WIDTH  720
#define DISP_HEIGHT 480

// 显示尺寸
int width = DISP_WIDTH;
int height = DISP_HEIGHT;

// 模型输入尺寸
int model_width = 224;
int model_height = 224;

// FrameBuffer全局变量
struct {
    int fd;
    uint16_t* mem;  // RGB565格式
    struct fb_var_screeninfo var_info;
} fb0;

// 初始化FrameBuffer
int init_framebuffer() {
    fb0.fd = open("/dev/fb0", O_RDWR);
    if (fb0.fd < 0) {
        perror("Open /dev/fb0 failed");
        return -1;
    }

    if (ioctl(fb0.fd, FBIOGET_VSCREENINFO, &fb0.var_info)) {
        perror("Get framebuffer info failed");
        close(fb0.fd);
        return -1;
    }

    size_t fb_size = fb0.var_info.xres * fb0.var_info.yres * 2;
    fb0.mem = (uint16_t*)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb0.fd, 0);
    if (fb0.mem == MAP_FAILED) {
        perror("mmap framebuffer failed");
        close(fb0.fd);
        return -1;
    }

    printf("Framebuffer initialized: %dx%d %dbpp\n", 
          fb0.var_info.xres, fb0.var_info.yres, 
          fb0.var_info.bits_per_pixel);
    return 0;
}

// BGR888转RGB565转换函数
void bgr888_to_rgb565(const cv::Mat& src, uint16_t* dst) {
    for (int y = 0; y < src.rows; ++y) {
        const cv::Vec3b* src_row = src.ptr<cv::Vec3b>(y);
        for (int x = 0; x < src.cols; ++x) {
            // OpenCV默认BGR顺序 → 转换为RGB
            uint8_t r = src_row[x][0];  // 原BGR中的R分量
            uint8_t g = src_row[x][1];
            uint8_t b = src_row[x][2];  // 原BGR中的B分量
            
            // RGB565编码（高位R[4:0], 中段G[5:3], 低位B[4:0]）
            dst[y * fb0.var_info.xres + x] = 
                ((r & 0xF8) << 8) |  // R: 取高5位（0xF8=11111000）
                ((g & 0xFC) << 3) |  // G: 取高6位（0xFC=11111100）
                (b >> 3);            // B: 取高5位
        }
    }
}

// 在显示帧上绘制结果
void draw_results(cv::Mat& frame, const mobilenet_result* results, char** labels) {
    const float FONT_SCALE = 0.4f;       // 缩小字体比例
    const int FONT_THICKNESS = 1;         // 减少线宽
    const int TEXT_MARGIN = 5;            // 边距缩小
    
    char text_buf[128];
    snprintf(text_buf, sizeof(text_buf), "Top1: %s (%.2f)", 
            labels[results[0].cls], results[0].score);
    
    int base_line;
    cv::Size text_size = cv::getTextSize(text_buf, cv::FONT_HERSHEY_SIMPLEX, 
                                       FONT_SCALE, FONT_THICKNESS, &base_line);
    
    // 右上角坐标计算
    int text_x = frame.cols - text_size.width - TEXT_MARGIN;
    int text_y = text_size.height + TEXT_MARGIN;
    
    // 绘制带背景的文本
    cv::rectangle(frame, 
                 cv::Point(text_x - 2, text_y - text_size.height - 2),
                 cv::Point(text_x + text_size.width + 2, text_y + 2),
                 cv::Scalar(255,255,255), -1); // 白色背景
    
    cv::putText(frame, text_buf, cv::Point(text_x, text_y),
               cv::FONT_HERSHEY_SIMPLEX, FONT_SCALE, 
               cv::Scalar(0, 0, 255),  // 红色字体
               FONT_THICKNESS);
}

int main(int argc, char** argv) {
    // 关闭rkicp
    system("RkLunch-stop.sh");
    
    // 标签加载
    int line_count;
    char** lines = read_lines_from_file(IMAGENET_CLASSES_FILE, &line_count);
    if (!lines) {
        printf("Error: Load label file failed\n");
        return -1;
    }

    // 模型初始化
    rknn_app_context_t model;
    if(init_mobilenet_model(MODEL_FILE, &model) != 0){
        printf("Model init failed!\n");
        free_lines(lines, line_count);
        return -1;
    }

    // 初始化摄像头ISP
    RK_BOOL multi_sensor = RK_FALSE;    // 单传感器模式
    const char *iq_dir = "/etc/iqfiles"; // IQ文件路径
    rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL; // HDR模式
    SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir); // ISP初始化
    SAMPLE_COMM_ISP_Run(0);  // 启动ISP

    // RKMPI初始化
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        RK_LOGE("rk mpi sys init fail!");
        return -1;
    }

    // VI初始化
    vi_dev_init();
    vi_chn_init(0, DISP_WIDTH, DISP_HEIGHT);

    // FrameBuffer初始化
    if (init_framebuffer() != 0) {
        printf("Init framebuffer failed!\n");
        return -1;
    }

    // 创建显示用Mat（240x240 RGB）
    cv::Mat display_frame(240, 240, CV_8UC3);

    // 识别用mat
    cv::Mat model_input(model_height, model_width, CV_8UC3, model.input_mems[0]->virt_addr);

    // 主循环
    while(1) {
        // 获取摄像头数据及推理部分
        VIDEO_FRAME_INFO_S stViFrame;
        RK_S32 ret = RK_MPI_VI_GetChnFrame(0, 0, &stViFrame, -1);
        
        if(ret == RK_SUCCESS) {
            // 获取原始YUV数据
            void* vi_data = RK_MPI_MB_Handle2VirAddr(stViFrame.stVFrame.pMbBlk);
            
            // 创建OpenCV Mat对象
            cv::Mat yuv420sp(stViFrame.stVFrame.u32Height * 3/2, stViFrame.stVFrame.u32Width, CV_8UC1, vi_data);
            
            // YUV420SP转BGR
            cv::Mat bgr_frame;
            cv::cvtColor(yuv420sp, bgr_frame, cv::COLOR_YUV2BGR_NV21);
            
            // 调整到模型输入尺寸
            cv::Mat resized_frame;
            cv::resize(bgr_frame, resized_frame, 
                      cv::Size(model_width, model_height), 
                      0, 0, cv::INTER_LINEAR);

            // 拷贝到模型输入内存
            memcpy(model_input.data, resized_frame.data, model_width * model_height * 3);

            // 执行推理
            mobilenet_result results[5];
            if(inference_mobilenet_model(&model, results, 5) == 0) {
                printf("\n----- Top5 Results -----\n");
                for(int i=0; i<5; ++i){
                    printf("[%d] score=%.6f class=%s\n", results[i].cls, results[i].score, lines[results[i].cls]);
                }
            }
            
            // 显示部分
            // 缩放原始图像到240x240
            cv::resize(bgr_frame, display_frame, cv::Size(240, 240));

            // 绘制推理结果
            draw_results(display_frame, results, lines);

            // 转换到RGB565并写入FrameBuffer
            bgr888_to_rgb565(display_frame, fb0.mem);

        }

        // 释放帧资源
        RK_MPI_VI_ReleaseChnFrame(0, 0, &stViFrame);
    }
    

    // 资源释放
    // 1. 关闭视频通道
    RK_MPI_VI_DisableChn(0, 0);
    RK_MPI_VI_DisableDev(0);
    
    // 2. 停止ISP
    SAMPLE_COMM_ISP_Stop(0);
    
    // 3. 释放模型
    release_mobilenet_model(&model);
    
    // 4. 释放标签
    free_lines(lines, line_count);
    
    return 0;
}
