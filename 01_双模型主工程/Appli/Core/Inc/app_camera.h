/**
 ****************************************************************************************************
 * @file        app_camera.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       app_camera.h文件
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#ifndef __APP_CAMERA_H
#define __APP_CAMERA_H

#include "stm32n6xx_hal.h"

int32_t app_camera_init(void);
int32_t app_camera_nn_pipe_start(uint8_t *nn_pipe_destination, uint32_t capture_mode);
void app_camera_nn_pipe_set_address(uint8_t *nn_pipe_destination);
uint32_t app_camera_nn_pipe_get_frame_sequence(void);
void app_camera_isp_update(void);

typedef struct {
    uint32_t pipe_error_count;
    uint32_t common_error_count;
    uint32_t csi_line_error_count;
    uint32_t last_error_code;
    uint32_t last_csi_sr0;
    uint32_t last_csi_sr1;
} app_camera_diag_t;

void app_camera_get_diag(app_camera_diag_t *diag);

#endif
