/**
 ****************************************************************************************************
 * @file        app_config.h
 * @brief       表情识别项目 — 摄像头配置
 *              基于 991 例程简化：去掉 LCD 显示层，只保留 NN pipe 输出
 ****************************************************************************************************
 */
#ifndef __APP_CONFIG_H
#define __APP_CONFIG_H

#include "stm32n6xx_hal.h"

/* ---- 摄像头配置 ---- */
#define CAMERA_MIRROR_FLIP          CMW_MIRRORFLIP_NONE   /* 镜像: 关闭 (改为 NONE) */

/* ---- NN Pipe 输出配置 (240×320 RGB888 → 与 RFB-320 输入尺寸完全匹配) ---- */
#define NN_WIDTH                    320
#define NN_HEIGHT                   240
#define NN_FORMAT                   DCMIPP_PIXEL_PACKER_FORMAT_RGB888_YUV444_1
#define NN_BPP                      3

/* ---- NN Pipe 缓冲区数量 (不需要 bqueue, 用单个 buffer) ---- */
#define NN_BUFFER_COUNT             1

#endif
