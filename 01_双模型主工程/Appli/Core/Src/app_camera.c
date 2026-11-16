/**
 ****************************************************************************************************
 * @file        app_camera.c
 * @brief       表情识别项目 — 摄像头驱动 (简化版)
 *              只保留 NN pipe (DCMIPP_PIPE2 → 224×224 RGB888)
 *              去掉 LCD 显示 pipe (DCMIPP_PIPE1) 和 ThreadX 依赖
 ****************************************************************************************************
 */
#include "app_camera.h"
#include "app_config.h"
#include "stm32n6xx_hal.h"
#include "cmw_camera.h"

static int32_t app_camera_nn_pipe_init(uint32_t sensor_width, uint32_t sensor_height);
static volatile uint32_t g_nn_frame_sequence = 0U;
static volatile uint32_t g_pipe_error_count = 0U;
static volatile uint32_t g_common_error_count = 0U;
static volatile uint32_t g_csi_line_error_count = 0U;
static volatile uint32_t g_last_error_code = 0U;
static volatile uint32_t g_last_csi_sr0 = 0U;
static volatile uint32_t g_last_csi_sr1 = 0U;

int32_t app_camera_init(void)
{
    CMW_CameraInit_t cmw_camera_init = {0};
    int32_t status;

    cmw_camera_init.width = 0;
    cmw_camera_init.height = 0;
    cmw_camera_init.fps = 0;
    cmw_camera_init.pixel_format = 0;
    cmw_camera_init.anti_flicker = 0;
    cmw_camera_init.mirror_flip = CAMERA_MIRROR_FLIP;
    status = CMW_CAMERA_Init(&cmw_camera_init);
    if (status != CMW_ERROR_NONE) {
        return status;
    }

    return app_camera_nn_pipe_init(cmw_camera_init.width, cmw_camera_init.height);
}

int32_t app_camera_nn_pipe_start(uint8_t *nn_pipe_destination, uint32_t capture_mode)
{
    int32_t status;

    g_nn_frame_sequence = 0U;
    g_pipe_error_count = 0U;
    g_common_error_count = 0U;
    g_csi_line_error_count = 0U;
    g_last_error_code = 0U;
    g_last_csi_sr0 = 0U;
    g_last_csi_sr1 = 0U;
    status = CMW_CAMERA_Start(DCMIPP_PIPE2, nn_pipe_destination, capture_mode);
    if (status != CMW_ERROR_NONE) {
        return status;
    }

    __HAL_DCMIPP_ENABLE_IT(CMW_CAMERA_GetDCMIPPHandle(),
                           DCMIPP_IT_PIPE2_FRAME | DCMIPP_IT_PIPE2_VSYNC |
                           DCMIPP_IT_PIPE2_OVR);
    return CMW_ERROR_NONE;
}

void app_camera_nn_pipe_set_address(uint8_t *nn_pipe_destination)
{
    HAL_DCMIPP_PIPE_SetMemoryAddress(CMW_CAMERA_GetDCMIPPHandle(),
                                     DCMIPP_PIPE2,
                                     DCMIPP_MEMORY_ADDRESS_0,
                                      (uint32_t)nn_pipe_destination);
}

uint32_t app_camera_nn_pipe_get_frame_sequence(void)
{
    return g_nn_frame_sequence;
}

void app_camera_isp_update(void)
{
    CMW_CAMERA_Run();
}

void app_camera_get_diag(app_camera_diag_t *diag)
{
    if (diag == NULL) {
        return;
    }

    diag->pipe_error_count = g_pipe_error_count;
    diag->common_error_count = g_common_error_count;
    diag->csi_line_error_count = g_csi_line_error_count;
    diag->last_error_code = g_last_error_code;
    diag->last_csi_sr0 = g_last_csi_sr0;
    diag->last_csi_sr1 = g_last_csi_sr1;
}

void CSI_IRQHandler(void)
{
    HAL_DCMIPP_CSI_IRQHandler(CMW_CAMERA_GetDCMIPPHandle());
}

void DCMIPP_IRQHandler(void)
{
    HAL_DCMIPP_IRQHandler(CMW_CAMERA_GetDCMIPPHandle());
}

static int32_t app_camera_nn_pipe_init(uint32_t sensor_width, uint32_t sensor_height)
{
    CMW_DCMIPP_Conf_t cmw_dcmipp_conf = {0};
    uint32_t hw_pitch;
    (void)sensor_width;
    (void)sensor_height;

    cmw_dcmipp_conf.output_width = NN_WIDTH;
    cmw_dcmipp_conf.output_height = NN_HEIGHT;
    cmw_dcmipp_conf.output_format = NN_FORMAT;
    cmw_dcmipp_conf.output_bpp = NN_BPP;
    cmw_dcmipp_conf.enable_swap = 1;
    cmw_dcmipp_conf.enable_gamma_conversion = 0;
    cmw_dcmipp_conf.mode = CMW_Aspect_ratio_fit;
    return CMW_CAMERA_SetPipeConfig(DCMIPP_PIPE2, &cmw_dcmipp_conf, &hw_pitch);
}

int CMW_CAMERA_PIPE_VsyncEventCallback(uint32_t pipe)
{
    (void)pipe;
    return 0;
}

int CMW_CAMERA_PIPE_FrameEventCallback(uint32_t pipe)
{
    if (pipe == DCMIPP_PIPE2) {
        g_nn_frame_sequence++;
    }
    return 0;
}

/* Keep error handling non-blocking: main.c reports these snapshots over UART. */
void HAL_DCMIPP_PIPE_ErrorCallback(DCMIPP_HandleTypeDef *hdcmipp, uint32_t pipe)
{
    if (pipe == DCMIPP_PIPE2) {
        g_pipe_error_count++;
        g_last_error_code = hdcmipp->ErrorCode;
    }
}

void HAL_DCMIPP_ErrorCallback(DCMIPP_HandleTypeDef *hdcmipp)
{
    g_common_error_count++;
    g_last_error_code = hdcmipp->ErrorCode;
}

void HAL_DCMIPP_CSI_LineErrorCallback(DCMIPP_HandleTypeDef *hdcmipp, uint32_t data_lane)
{
    (void)data_lane;
    g_csi_line_error_count++;
    g_last_error_code = hdcmipp->ErrorCode;
    g_last_csi_sr0 = CSI->SR0;
    g_last_csi_sr1 = CSI->SR1;
}
