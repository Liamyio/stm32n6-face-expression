/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 鐞涖劍鍎忕拠鍡楀焼缁崵绮?- 閸欏本膩閸?AI (娴滈缚鍔Λ鈧ù?鐞涖劍鍎忕拠鍡楀焼)
  *                   濮濓絿鍋ｉ崢鐔风摍 STM32N647, UART 鏉堟挸鍤? 閺冪娀娓剁仦蹇撶
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* NPU Runtime 閳?韫囧懘銆忛崷?HAL/CMSIS 娑斿鎮?include */
#include "ai_platform.h"
#include "ll_aton.h"
#include "ll_aton_runtime.h"

/* RISAF 闁板秶鐤嗛棁鈧憰?HAL RIF */
#include "stm32n6xx_hal_rif.h"
/* NPU Cache */
#include "npu_cache.h"

/* ---- 鐞涖劍鍎忕拠鍡楀焼濡€崇€?(ResNet18 INT8) ---- */
#include "network.h"           /* LL_ATON_NETWORK_*  */
#include "stai.h"              /* stai_runtime_*      */
#include "stai_network.h"      /* stai_network_*      */
/* ATON ST.AI extension is provided by the runtime but not declared by the
   generated public headers in this SDK package. */
extern stai_return_code stai_ext_wfe(void);

/* ---- 娴滈缚鍔Λ鈧ù瀣侀崹?(RFB-320 INT8, 缁楋箑褰块崜宥囩磻 FACENET) ---- */
#include "facenet_network.h"   /* LL_ATON_FACENET_*  */
#include "facenet_stai.h"      /* stai_facenet_*     */

/* ---- 閹藉嫬鍎氭径鎾攳閸?(IMX335) ---- */
#include "app_camera.h"
#include "app_config.h"
#include "cmw_camera.h"
#include "cmw_imx335.h"
#include "cmw_io.h"
#include "imx335_reg.h"

/* Private typedef -----------------------------------------------------------*/

/* ---- NMS 閸婃瑩鈧顢?---- */
typedef struct {
    float x1, y1, x2, y2;   /* 瑜版帊绔撮崠鏍ф綏閺?[0,1] */
    float score;
} FaceBox;

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 鐞涖劍鍎忕猾璇插焼 */
static const char *emotion_names[] = {
    "愤怒", "厌恶", "恐惧", "开心",
    "中性", "悲伤", "惊讶"
};

/* NMS 閸欏倹鏆?(RFB-320 鏉堟挸鍤?4420 閸婃瑩鈧?閳?閺堚偓婢舵矮绻氶悾?20 娑擃亙姹夐懘? */
#define FACE_MAX_CANDIDATES  4420
#define FACE_MAX_OUTPUTS     20
#define FACE_CONF_THRESHOLD  0.5f     /* 缂冾喕淇婃惔锕傛閸?*/
#define FACE_IOU_THRESHOLD   0.45f    /* NMS IOU 闂冨牆鈧?*/
#define FACE_IMAGE_VARIANCE  0.01f    /* 閺傜懓妯婇梼鍫濃偓?(閸ュ墽澧栭弬鐟版▕鐏忓繋绨銈呪偓鑹邦潒娑撻缚鍎楅弲? */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

UART_HandleTypeDef huart1;
volatile uint32_t g_fault_stage = 0U;
I2C_HandleTypeDef hi2c2;        /* IMX335 閹藉嫬鍎氭径缈犲▏閻劎娈?I2C2 */

/* Captured by the fault handler to identify the active foreground operation. */

/* ---- 婢圭増妲戞稉銈勯嚋濡€崇€烽惃?NPU 鐎圭偘绶?---- */
LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(network);   /* 鐞涖劍鍎忕拠鍡楀焼 */
LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(facenet);   /* 娴滈缚鍔Λ鈧ù?*/

/* 缂冩垹绮舵稉濠佺瑓閺?*/
AI_ALIGNED(STAI_NETWORK_CONTEXT_ALIGNMENT)
/* Keep the canary physically after the context. Separate globals may be
   reordered by the linker and cannot diagnose a forward overwrite. */
typedef struct {
    uint8_t ctx[STAI_NETWORK_CONTEXT_SIZE];
    /* ATON runtime writes execution bookkeeping beyond the generated public
       context size for this model; keep that spill isolated from peripherals. */
    uint8_t runtime_slack[256];
} ExprContextStorage;
AI_ALIGNED(STAI_NETWORK_CONTEXT_ALIGNMENT)
static ExprContextStorage expr_storage;
#define expr_ctx       (expr_storage.ctx)
AI_ALIGNED(STAI_NETWORK_CONTEXT_ALIGNMENT)
static uint8_t face_ctx[STAI_FACENET_CONTEXT_SIZE];

/* ---- 娴滈缚鍔Λ鈧ù? 鏉堟挸鍙?(240x320x3 int8 CHW, scale=0.0078125, zp=-1) ---- */
AI_ALIGNED(32)
static int8_t face_input[3 * 240 * 320];   /* 230,400 bytes */

/* ---- 娴滈缚鍔Λ鈧ù? 鏉堟挸鍤?buffer (int8, 闂団偓閸欏秹鍣洪崠? ---- */
AI_ALIGNED(32)
static int8_t face_scores[LL_ATON_FACENET_OUT_1_SIZE_BYTES];  /* 8840 int8 */
AI_ALIGNED(32)
static int8_t face_boxes[LL_ATON_FACENET_OUT_2_SIZE_BYTES];   /* 17680 int8 */

/* ---- NMS 娑撳瓨妞傜紓鎾冲暱 (闂堟瑦鈧礁鍨庨柊宥変缉閸?106KB 閺嶅牊瀛╅崙? ---- */
static FaceBox nms_candidates[FACE_MAX_CANDIDATES];
static int nms_kept_flags[FACE_MAX_CANDIDATES];
static FaceBox detected_faces[FACE_MAX_OUTPUTS];
static int detected_count = 0;

/* ---- 鐞涖劍鍎? 鏉堟挸鍙?(224x224x3 int8 CHW) ---- */
AI_ALIGNED(32)
static int8_t expr_input[3 * 224 * 224];   /* 150,528 bytes */

/* 鐞涖劍鍎忔潏鎾冲毉: 7 int8 (logits) */
AI_ALIGNED(32)
static int8_t expr_output[7];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void SystemIsolation_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C2_Init(void);
static void NPU_Clock_Init(void);
static void NPU_Cache_Init(void);
static void RISAF_Config(void);
static void NPU_RIF_Config(void);
static void SystemInit_NPU(void);
static void print_npu_timeout_diag(void);

/* USER CODE BEGIN PFP */
/* ---- 鐞涖劍鍎忕拠鍡楀焼 ---- */
static void run_expression(const int8_t *img, int8_t *out);
static void softmax7(float *v);
static int  argmax7(const float *v);

/* ---- 娴滈缚鍔Λ鈧ù?---- */
static void run_face_detect(const int8_t *img, int8_t *scores, int8_t *boxes);
static int  face_nms(const int8_t *scores, const int8_t *boxes_top4,
                     FaceBox *out, int max_out);
static float face_iou(const FaceBox *a, const FaceBox *b);
static void quantize_face_to_expr(const int8_t *face_rgb, int face_w, int face_h,
                                  float x1, float y1, float x2, float y2,
                                  int8_t *expr_224);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* printf 閳?USART1 */
int _write(int fd, const char *ptr, int len)
{
    (void)fd;
    for (int i = 0; i < len; i++) {
        while (!(USART1->ISR & USART_ISR_TXE_TXFNF));
        USART1->TDR = *ptr++;
    }
    return len;
}

/* NPU trace stub */
void ec_trace_unsupported(void)
{
    printf("[NPU] ec_trace_unsupported called!\r\n");
}

void ec_trace_write(uintptr_t dstreg, unsigned int val)
{
    *(volatile unsigned int *)dstreg = val;
}

/* ---- NPU 閻妫悪?---- */
#define WDOG_TIMEOUT_MS  120000U  /* FP32 娴滈缚鍔Λ鈧ù瀣付婢?120 缁?*/
static volatile uint32_t wdog_started_ms = 0U;
static volatile uint32_t wdog_timeout_ms = 0U;

int startWatchdog(uint32_t timeout_cycles)
{
    uint32_t requested_ms = timeout_cycles / 1000U;
    wdog_timeout_ms = (requested_ms > WDOG_TIMEOUT_MS) ? requested_ms : WDOG_TIMEOUT_MS;
    wdog_started_ms = HAL_GetTick();
    return 0;
}

int checkWatchdog(void)
{
    uint32_t elapsed_ms;
    if (wdog_timeout_ms == 0U) return 0;
    elapsed_ms = HAL_GetTick() - wdog_started_ms;
    if (elapsed_ms >= wdog_timeout_ms) {
        printf("\r\n[FATAL] NPU epoch timeout (>%lu ms)!\r\n",
               (unsigned long)wdog_timeout_ms);
        print_npu_timeout_diag();
        wdog_timeout_ms = 0U;
        return -1;
    }
    return 0;
}

/* ---- 閻㈢喐鍨氬ù瀣槸閸ユ儳鍎?(240x320 int8, scale=0.0078125, zp=-1) ---- */
/* Legacy synthetic-image helper removed; camera frames are the only input. */
/* USER CODE END 0 */

/* ================================================================
 *  main
 * ================================================================ */
int main(void)
{
    /* USER CODE BEGIN 1 */
    /* USER CODE END 1 */

    CLEAR_BIT(SCB->SCR, SCB_SCR_SLEEPDEEP_Msk);
    SCB_EnableICache();
    SCB_EnableDCache();

    HAL_Init();

    /* USER CODE BEGIN Init */
    /* USER CODE END Init */

    /* ---- NPU 閸╄櫣顢呯拋鐐煢閸掓繂顫愰崠?---- */
    SystemInit_NPU();

    /* ---- 婢舵牞顔?RIF + GPIO + USART + I2C2 ---- */
    SystemIsolation_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_I2C2_Init();

    /* ---- RISAF + NPU RIF 鐎瑰鍙忛柊宥囩枂 ---- */
    __HAL_RCC_RISAF_CLK_ENABLE();
    RISAF_Config();
    NPU_RIF_Config();

    /* ---- 閸忋劌鐪?NPU Runtime 閸掓繂顫愰崠?(韫囧懘銆忛崷銊ゆ崲娴ｆ洘膩閸ㄥ澧? ---- */
    {
        stai_return_code rc = stai_runtime_init();
        if (rc != STAI_SUCCESS) {
            printf("[FATAL] stai_runtime_init 婢惰精瑙? %d\r\n", (int)rc);
            while (1);
        }
        printf("[INIT] STAI Runtime OK\r\n");
    }

    /* USER CODE BEGIN 2 */

    printf("\r\n\r\n");
    printf("========================================\r\n");
    printf("   STM32N6 双模型AI系统\r\n");
    printf("========================================\r\n");
    printf("   表情模型: ResNet18 INT8 (54/54 HW)\r\n");
    printf("   人脸模型: RFB-320 INT8 (52/65 HW)\r\n");
    printf("   摄像头: IMX335\r\n");
    printf("   串口: USART1 (115200)\r\n");
    printf("========================================\r\n\r\n");

    /* ---- 閹藉嫬鍎氭径鏉戝灥婵瀵?(IMX335 + DCMIPP + ISP) ---- */
    printf("[摄像头] 初始化IMX335...\r\n");
    int32_t camera_status = app_camera_init();
    if (camera_status != CMW_ERROR_NONE) {
        printf("[CAM-ERROR] 閸掓繂顫愰崠鏍ㄥ灗 PIPE2 闁板秶鐤嗘径杈Е: %ld\r\n", (long)camera_status);
        Error_Handler();
    }
    printf("[摄像头] 初始化成功\r\n");

    /*
     *  閹藉嫬鍎氭径纾嬬翻閸戣櫣顓哥痪?
     *  IMX335 (2592鑴?944 RAW10)
     *    閳?DCMIPP_CSI (MIPI 2-lane)
     *    閳?DCMIPP_PIPE2 (downsize 閳?320鑴?40 RGB888)
     *    閳?camera_buf (HWC, 320鑴?40鑴? bytes)
     *    閳?HWC閳墫HW + int8 闁插繐瀵叉潪顒佸床 閳?face_input
     */
    printf("[摄像头] 启动连续采集...\r\n");
    /* 娴ｈ法鏁ゆ径鏍劥 SRAM 娑擃厾娈戞稉瀛樻缂傛挸鍟块崠鐑樺复閺€?DCMIPP 鏉堟挸鍤?(HWC 鐢啫鐪? */
    static uint8_t camera_buf[320 * 240 * 3] __attribute__((aligned(32)));
    memset(camera_buf, 0, sizeof(camera_buf));
    SCB_CleanDCache_by_Addr((uint32_t *)camera_buf, sizeof(camera_buf));
    camera_status = CMW_CAMERA_SetTestPattern(-1);
    if (camera_status != CMW_ERROR_NONE) {
        printf("[CAM-ERROR] IMX335 test pattern disable failed: %ld\r\n", (long)camera_status);
        Error_Handler();
    }
    camera_status = CMW_CAMERA_SetExposure(IMX335_EXPOSURE_DEFAULT);
    if (camera_status != CMW_ERROR_NONE) {
        printf("[CAM-ERROR] IMX335 initial exposure setup failed: %ld\r\n", (long)camera_status);
        Error_Handler();
    }
    camera_status = CMW_CAMERA_SetGain(IMX335_GAIN_DEFAULT);
    if (camera_status != CMW_ERROR_NONE) {
        printf("[CAM-ERROR] IMX335 initial gain setup failed: %ld\r\n", (long)camera_status);
        Error_Handler();
    }
    camera_status = app_camera_nn_pipe_start(camera_buf, DCMIPP_MODE_CONTINUOUS);
    if (camera_status != CMW_ERROR_NONE) {
        printf("[CAM-ERROR] PIPE2 閸氼垰濮╂径杈Е: %ld\r\n", (long)camera_status);
        Error_Handler();
    }
    /*
     * 妫板嫯顓哥粻?float 瑜版帊绔撮崠鏍ㄧ叀閹垫崘銆? HWC buf[0..227] 閳?int8 CHW
     * 濡€崇€烽柌蹇撳閸欏倹鏆? scale=0.0078125=1/128, zp=-1
     * i8 = round(val / scale) + zp = round(val * 128) - 1
     * 缁犫偓閸栨牔璐熺悰? buf8[k] = lut[val] where lut[x] = round(x * 128) - 1 clamped to [-128, 127]
     */
    /* RGB byte values must be normalized to [-1, 1] before face INT8 quantization. */
    static int8_t hwc_lut[256] __attribute__((aligned(32)));
    static int lut_init = 0;
    if (!lut_init) {
        for (int i = 0; i < 256; i++) {
            int v = (int)roundf((float)i * (256.0f / 255.0f) - 129.0f);
            if (v > 127) v = 127;
            if (v < -128) v = -128;
            hwc_lut[i] = (int8_t)v;
        }
        lut_init = 1;
    }
    /* ---- Loop-local constants for HWC閳墫HW conversion ---- */
    const int H = 240, W = 320, C = 3;
    const uint32_t camera_buf_size = H * W * C;
    uint32_t camera_frame_sequence = app_camera_nn_pipe_get_frame_sequence();

    printf("[摄像头] AE/AWB预热，等待30帧...\r\n");
    {
        uint32_t warmup_start = HAL_GetTick();
        uint32_t warmup_first_sequence = camera_frame_sequence;
        while ((app_camera_nn_pipe_get_frame_sequence() - warmup_first_sequence) < 30U) {
            app_camera_isp_update();
            if ((HAL_GetTick() - warmup_start) >= 5000U) {
                printf("[CAM-WARN] AE/AWB warm-up timeout after %lu frames\r\n",
                       (unsigned long)(app_camera_nn_pipe_get_frame_sequence() - warmup_first_sequence));
                break;
            }
        }
        camera_frame_sequence = app_camera_nn_pipe_get_frame_sequence();
        printf("[摄像头] AE/AWB预热完成: %lu帧\r\n",
               (unsigned long)(camera_frame_sequence - warmup_first_sequence));
    }
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_10);   /* LED: 閹恒劎鎮婂鈧慨?*/

        /* ---- 缁涘绶熼幗鍕剼婢跺瓨鏌婄敮?(ISP 閸氬骸褰撮弴瀛樻煀) ---- */
        /* Wait for DCMIPP PIPE2 to finish a new DMA frame before reading it. */
        uint32_t frame_wait_started = HAL_GetTick();
        uint32_t next_frame_sequence;
        do {
            app_camera_isp_update();
            next_frame_sequence = app_camera_nn_pipe_get_frame_sequence();
            if ((HAL_GetTick() - frame_wait_started) >= 2000U) {
                printf("[CAM-ERROR] PIPE2 frame timeout\r\n");
                break;
            }
        } while (next_frame_sequence == camera_frame_sequence);
        if (next_frame_sequence == camera_frame_sequence) {
            continue;
        }
        camera_frame_sequence = next_frame_sequence;

        if (CMW_CAMERA_Suspend(DCMIPP_PIPE2) != CMW_ERROR_NONE) {
            printf("[CAM-ERROR] PIPE2 suspend failed\r\n");
            continue;
        }

        /* DCMIPP DMA updates camera_buf while the CPU cache is enabled. */
        SCB_InvalidateDCache_by_Addr((uint32_t *)camera_buf, camera_buf_size);

        /* ---- HWC閳墫HW 鏉烆剚宕? camera_buf (RGBRGB...) 閳?face_input (RRR...GGG...BBB...) ---- */
        uint8_t *hwc = camera_buf;
        int8_t *chw = face_input;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                uint8_t r = hwc[y * W * C + x * C + 0];
                uint8_t g = hwc[y * W * C + x * C + 1];
                uint8_t b = hwc[y * W * C + x * C + 2];
                chw[0 * H * W + y * W + x] = hwc_lut[r];
                chw[1 * H * W + y * W + x] = hwc_lut[g];
                chw[2 * H * W + y * W + x] = hwc_lut[b];
            }
        }

        if (CMW_CAMERA_Resume(DCMIPP_PIPE2) != CMW_ERROR_NONE) {
            printf("[CAM-ERROR] PIPE2 resume failed\r\n");
            continue;
        }

        /* ================================================
         *  濮濄儵顎?1: 娴滈缚鍔Λ鈧ù?(RFB-320)
         * ================================================ */
        printf("========================================\r\n");
        printf("[人脸] 检测中...\r\n");

        uint32_t t0 = HAL_GetTick();
        run_face_detect(face_input, face_scores, face_boxes);
        uint32_t t1 = HAL_GetTick();

        printf("[人脸] 推理耗时: %lu ms\r\n", (unsigned long)(t1 - t0));

        /* NMS 閸氬骸顦╅悶? 4420 閸婃瑩鈧?閳?閺堚偓婢?20 娑擃亝顢?*/
        detected_count = face_nms(face_scores, face_boxes,
                                  detected_faces, FACE_MAX_OUTPUTS);
        printf("[人脸] 检测到 %d 张人脸:\r\n", detected_count);

        for (int fi = 0; fi < detected_count; fi++) {
            printf("       人脸%d: 置信度=%.2f  (%.3f,%.3f)-(%.3f,%.3f)\r\n",
                   fi + 1,
                   (double)detected_faces[fi].score,
                   (double)detected_faces[fi].x1,
                   (double)detected_faces[fi].y1,
                   (double)detected_faces[fi].x2,
                   (double)detected_faces[fi].y2);
        }

        /* ================================================
         *  濮濄儵顎?2: 鐎佃鐦℃稉顏冩眽閼寸浠涚悰銊﹀剰鐠囧棗鍩?
         * ================================================ */
        if (detected_count > 0) {
            /* 閸欐牜鐤嗘穱鈥冲閺堚偓妤傛娈戞禍楦垮姱閸嬫俺銆冮幆鍛扮槕閸?*/
            int best_idx = 0;
            for (int fi = 1; fi < detected_count; fi++) {
                if (detected_faces[fi].score > detected_faces[best_idx].score)
                    best_idx = fi;
            }

            FaceBox *face = &detected_faces[best_idx];
            printf("\r\n[表情] 识别第%d张人脸...\r\n", best_idx + 1);

            /* 鐟佷礁澹€ + 缂傗晜鏂?+ 闁插繐瀵?*/
            quantize_face_to_expr(face_input, 320, 240,
                                  face->x1, face->y1, face->x2, face->y2,
                                  expr_input);

            /* 鐞涖劍鍎忛幒銊ф倞 */
            uint32_t t2 = HAL_GetTick();
            run_expression(expr_input, expr_output);
            uint32_t t3 = HAL_GetTick();

            printf("[表情] 推理耗时: %lu ms\r\n", (unsigned long)(t3 - t2));

            /* 閸欏秹鍣洪崠? out_f = (out_i8 + 32) * 0.019689815 */
            float logits[7];
            for (int i = 0; i < 7; i++)
                logits[i] = (float)((int)expr_output[i] + 32) * 0.019689815f;

            softmax7(logits);
            int top = argmax7(logits);

            printf("[表情] 结果: %s (%.1f%%)\r\n",
                   emotion_names[top], (double)(logits[top] * 100.0f));
            printf("[表情] 各类别概率: ");
            for (int i = 0; i < 7; i++)
                printf("%s=%.1f  ", emotion_names[i], (double)(logits[i] * 100.0f));
            printf("\r\n");
        } else {
            printf("[表情] 未检测到人脸，跳过识别\r\n");
        }

        uint32_t total_ms = HAL_GetTick() - t0;
        printf("----------------------------------------\r\n");
        printf("[总耗时] %lu ms\r\n", (unsigned long)total_ms);
        printf("========================================\r\n\r\n");

        HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_10);   /* LED: 閹恒劎鎮婄€瑰本鍨?*/
        HAL_Delay(2000);

        /* USER CODE END WHILE */
        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/* ================================================================
 *  NPU 閸掓繂顫愰崠? * ================================================================ */

static void SystemInit_NPU(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_CRC_CLK_ENABLE();

    RCC->MEMENR |= RCC_MEMENR_AXISRAM3EN |
                   RCC_MEMENR_AXISRAM4EN |
                   RCC_MEMENR_AXISRAM5EN |
                   RCC_MEMENR_AXISRAM6EN;
    RCC->MEMENR |= RCC_MEMENR_CACHEAXIRAMEN;

    RAMCFG_SRAM2_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    RAMCFG_SRAM3_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    RAMCFG_SRAM4_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    RAMCFG_SRAM5_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    RAMCFG_SRAM6_AXI->CR &= ~RAMCFG_CR_SRAMSD;

    MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_DCACTIVE_Msk |
                       MEMSYSCTL_MSCR_ICACTIVE_Msk;

    NPU_Clock_Init();
    NPU_Cache_Init();
}

static void NPU_Clock_Init(void)
{
    __HAL_RCC_NPU_CLK_ENABLE();
    __HAL_RCC_NPU_FORCE_RESET();
    __HAL_RCC_NPU_RELEASE_RESET();

    __HAL_RCC_CACHEAXI_CLK_ENABLE();
    __HAL_RCC_CACHEAXI_FORCE_RESET();
    __HAL_RCC_CACHEAXI_RELEASE_RESET();

    __HAL_RCC_IAC_CLK_ENABLE();
    __HAL_RCC_IAC_FORCE_RESET();
    __HAL_RCC_IAC_RELEASE_RESET();
}

static void NPU_Cache_Init(void)
{
    npu_cache_enable();
}

void npu_cache_enable_clocks_and_reset(void)
{
    __HAL_RCC_CACHEAXI_CLK_ENABLE();
    __HAL_RCC_CACHEAXI_FORCE_RESET();
    __HAL_RCC_CACHEAXI_RELEASE_RESET();
}

void npu_cache_disable_clocks_and_reset(void)
{
    __HAL_RCC_CACHEAXI_FORCE_RESET();
    __HAL_RCC_CACHEAXI_RELEASE_RESET();
    __HAL_RCC_CACHEAXI_CLK_DISABLE();
}

/* ---- RISAF 鏉堝懎濮崙鑺ユ殶 ---- */
static uint32_t get_risaf_max_addr(RISAF_TypeDef *risaf)
{
    if      (risaf == RISAF1_S  || risaf == RISAF1_NS)  return RISAF1_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF2_S  || risaf == RISAF2_NS)  return RISAF2_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF3_S  || risaf == RISAF3_NS)  return RISAF3_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF4_S  || risaf == RISAF4_NS)  return RISAF4_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF5_S  || risaf == RISAF5_NS)  return RISAF5_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF6_S  || risaf == RISAF6_NS)  return RISAF6_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF7_S  || risaf == RISAF7_NS)  return RISAF7_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF8_S  || risaf == RISAF8_NS)  return RISAF8_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF9_S  || risaf == RISAF9_NS)  return RISAF9_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF11_S || risaf == RISAF11_NS) return RISAF11_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF12_S || risaf == RISAF12_NS) return RISAF12_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF13_S || risaf == RISAF13_NS) return RISAF13_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF14_S || risaf == RISAF14_NS) return RISAF14_LIMIT_ADDRESS_SPACE_SIZE;
    else if (risaf == RISAF15_S || risaf == RISAF15_NS) return RISAF15_LIMIT_ADDRESS_SPACE_SIZE;
    return 0;
}

static void set_risaf_default(RISAF_TypeDef *risaf)
{
    RISAF_BaseRegionConfig_t risaf_conf;
    risaf_conf.StartAddress   = 0x0;
    risaf_conf.EndAddress     = get_risaf_max_addr(risaf);
    risaf_conf.Filtering      = RISAF_FILTER_ENABLE;
    risaf_conf.PrivWhitelist  = RIF_CID_NONE;
    risaf_conf.ReadWhitelist  = RIF_CID_MASK;
    risaf_conf.WriteWhitelist = RIF_CID_MASK;

    risaf_conf.Secure = RIF_ATTRIBUTE_SEC;
    HAL_RIF_RISAF_ConfigBaseRegion(risaf, 0, &risaf_conf);
    risaf_conf.Secure = RIF_ATTRIBUTE_NSEC;
    HAL_RIF_RISAF_ConfigBaseRegion(risaf, 1, &risaf_conf);
}

static void RISAF_Config(void)
{
    set_risaf_default(RISAF4_S);
    set_risaf_default(RISAF5_S);
    set_risaf_default(RISAF6_S);
    set_risaf_default(RISAF8_S);
    set_risaf_default(RISAF15_S);
    set_risaf_default(RISAF11_S);
    set_risaf_default(RISAF12_S);
}

static void NPU_RIF_Config(void)
{
    RIMC_MasterConfig_t master_conf;
    master_conf.MasterCID = RIF_CID_1;
    master_conf.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &master_conf);
    HAL_RIF_RISC_SetSlaveSecureAttributes(
        RIF_RISC_PERIPH_INDEX_NPU,
        RIF_ATTRIBUTE_PRIV | RIF_ATTRIBUTE_SEC);
}

static void print_npu_timeout_diag(void)
{
    printf("[NPU-ERROR] inference timeout\r\n");
}

/* ================================================================
 *  婢舵牞顔曢崚婵嗩潗閸? * ================================================================ */

static void SystemIsolation_Config(void)
{
    __HAL_RCC_RIFSC_CLK_ENABLE();

    RIMC_MasterConfig_t camera_master = {0};
    camera_master.MasterCID = RIF_CID_1;
    camera_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_DCMIPP, &camera_master);
    HAL_RIF_RISC_SetSlaveSecureAttributes(
        RIF_RISC_PERIPH_INDEX_DCMIPP, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(
        RIF_RISC_PERIPH_INDEX_CSI, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
    HAL_RIF_RISC_SetSlaveSecureAttributes(
        RIF_RISC_PERIPH_INDEX_I2C2, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);

    /* IMX335 I2C control bus and sensor reset/enable pins. */
    HAL_GPIO_ConfigPinAttributes(GPIOD, GPIO_PIN_4 | GPIO_PIN_14,
                                 GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_4 | GPIO_PIN_6,
                                 GPIO_PIN_SEC | GPIO_PIN_NPRIV);

    /* LED pins */
    HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_10,
                                 GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_10,
                                 GPIO_PIN_SEC | GPIO_PIN_NPRIV);

    /* USART1 pins */
    HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_5,
                                 GPIO_PIN_SEC | GPIO_PIN_NPRIV);
    HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_6,
                                 GPIO_PIN_SEC | GPIO_PIN_NPRIV);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10, GPIO_PIN_RESET);
    g.Pin = GPIO_PIN_10;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &g);

    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_RESET);
    g.Pin = GPIO_PIN_10;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &g);
}

static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_5;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOE, &g);

    g.Pin = GPIO_PIN_6;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOE, &g);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    HAL_UART_Init(&huart1);
}

/* ================================================================
 *  AI 閹恒劎鎮婇弽绋跨妇
 * ================================================================ */

/* ---- 鐞涖劍鍎忕拠鍡楀焼 (ResNet18 INT8) ---- */
static int expr_initialized = 0;

static void run_expression(const int8_t *img, int8_t *out)
{
    stai_return_code rc;
    stai_network *net = (stai_network *)expr_ctx;

    if (!expr_initialized) {
        printf("  [表情] 初始化识别网络...\r\n");
        rc = stai_network_init(net);
        if (rc != STAI_SUCCESS) {
            printf("  [EXPR-ERROR] stai_network_init 婢惰精瑙? %d\r\n", (int)rc);
            return;
        }
        expr_initialized = 1;
        printf("  [表情] 初始化成功\r\n");
    }

    stai_ptr in_bufs[1];
    stai_size n_in = 1;
    rc = stai_network_get_inputs(net, in_bufs, &n_in);
    if (rc != STAI_SUCCESS) {
        printf("  [EXPR-ERROR] get_inputs 婢惰精瑙? %d\r\n", (int)rc);
        return;
    }
    int8_t *in_ptr = (int8_t *)in_bufs[0];
    uint32_t in_size = LL_ATON_NETWORK_IN_1_SIZE_BYTES;
    memcpy(in_ptr, img, in_size);

    SCB_CleanDCache_by_Addr((uint32_t *)in_ptr, in_size);

    rc = stai_network_run(net, STAI_MODE_ASYNC);
    if (rc != STAI_RUNNING_NO_WFE && rc != STAI_RUNNING_WFE && rc != STAI_DONE) {
        printf("  [EXPR-ERROR] stai_network_run 婢惰精瑙? %d\r\n", (int)rc);
        return;
    }
    for (;;) {
        stai_return_code status = stai_ext_network_get_nn_run_status(net);
        if (status == STAI_DONE) {
            break;
        }
        if (status == STAI_RUNNING_WFE) {
            (void)stai_ext_wfe();
        } else if (status != STAI_RUNNING_NO_WFE) {
            printf("  [EXPR-ERROR] inference status: %d\r\n", (int)status);
            return;
        }
        rc = stai_ext_network_run_continue(net);
        if (rc != STAI_RUNNING_NO_WFE && rc != STAI_RUNNING_WFE && rc != STAI_DONE) {
            printf("  [EXPR-ERROR] inference continue: %d\r\n", (int)rc);
            return;
        }
    }
    rc = stai_ext_network_new_inference(net);
    if (rc != STAI_SUCCESS) {
        printf("  [EXPR-ERROR] inference reset: %d\r\n", (int)rc);
        return;
    }

    stai_ptr out_bufs[1];
    stai_size n_out = 1;
    rc = stai_network_get_outputs(net, out_bufs, &n_out);
    if (rc != STAI_SUCCESS) {
        printf("  [EXPR-ERROR] get_outputs 婢惰精瑙? %d\r\n", (int)rc);
        return;
    }
    int8_t *out_ptr = (int8_t *)out_bufs[0];
    uint32_t out_size = LL_ATON_NETWORK_OUT_1_SIZE_BYTES;

    SCB_InvalidateDCache_by_Addr((uint32_t *)out_ptr, out_size);
    memcpy(out, out_ptr, out_size);

    /* The generated context is overwritten by ATON bookkeeping beyond its
       public size. Recreate it for every frame so stale state cannot poison
       the next inference. The runtime spill area remains isolated. */
    memset(expr_ctx, 0, STAI_NETWORK_CONTEXT_SIZE);
    expr_initialized = 0;
}

static void softmax7(float *v)
{
    float m = v[0];
    for (int i = 1; i < 7; i++)
        if (v[i] > m) m = v[i];

    float s = 0.0f;
    for (int i = 0; i < 7; i++) {
        v[i] = expf(v[i] - m);
        s += v[i];
    }

    float inv = (s > 0.0f) ? (1.0f / s) : (1.0f / 7.0f);
    for (int i = 0; i < 7; i++)
        v[i] *= inv;
}

static int argmax7(const float *v)
{
    int best = 0;
    for (int i = 1; i < 7; i++)
        if (v[i] > v[best]) best = i;
    return best;
}

/* ---- 娴滈缚鍔Λ鈧ù?(RFB-320 INT8) ---- */
static int face_initialized = 0;

static void run_face_detect(const int8_t *img, int8_t *scores, int8_t *boxes)
{
    stai_return_code rc;
    stai_network *net = (stai_network *)face_ctx;

    if (!face_initialized) {
        printf("  [人脸] 初始化检测网络...\r\n");
        rc = stai_facenet_init(net);
        if (rc != STAI_SUCCESS) {
            printf("  [FACE-ERROR] stai_facenet_init 婢惰精瑙? %d\r\n", (int)rc);
            return;
        }
        face_initialized = 1;
        printf("  [人脸] 初始化成功\r\n");
    }

    /* 閼惧嘲褰囨潏鎾冲弳 buffer */
    stai_ptr in_bufs[1];
    stai_size n_in = 1;
    rc = stai_facenet_get_inputs(net, in_bufs, &n_in);
    if (rc != STAI_SUCCESS) {
        printf("  [FACE-ERROR] get_inputs 婢惰精瑙? %d\r\n", (int)rc);
        return;
    }
    int8_t *in_ptr = (int8_t *)in_bufs[0];
    uint32_t in_size = LL_ATON_FACENET_IN_1_SIZE_BYTES;

    memcpy(in_ptr, img, in_size);

    SCB_CleanDCache_by_Addr((uint32_t *)in_ptr, in_size);

    uint32_t t_before = HAL_GetTick();
    rc = stai_facenet_run(net, STAI_MODE_SYNC);
    uint32_t t_after = HAL_GetTick();

    if (rc != STAI_SUCCESS) {
        printf("  [FACE-ERROR] stai_facenet_run 婢惰精瑙? %d\r\n", (int)rc);
        return;
    }

    /* 閼惧嘲褰囨潏鎾冲毉 buffers */
    stai_ptr out_bufs[2];
    stai_size n_out = 2;
    rc = stai_facenet_get_outputs(net, out_bufs, &n_out);
    if (rc != STAI_SUCCESS) {
        printf("  [FACE-ERROR] get_outputs 婢惰精瑙? %d\r\n", (int)rc);
        return;
    }
    int8_t *scores_ptr = (int8_t *)out_bufs[0];
    int8_t *boxes_ptr  = (int8_t *)out_bufs[1];

    SCB_InvalidateDCache_by_Addr((uint32_t *)scores_ptr, LL_ATON_FACENET_OUT_1_SIZE_BYTES);
    SCB_InvalidateDCache_by_Addr((uint32_t *)boxes_ptr,  LL_ATON_FACENET_OUT_2_SIZE_BYTES);

    memcpy(scores, scores_ptr, LL_ATON_FACENET_OUT_1_SIZE_BYTES);
    memcpy(boxes,  boxes_ptr,  LL_ATON_FACENET_OUT_2_SIZE_BYTES);
}

/* ---- NMS 閸氬骸顦╅悶?---- */

static float face_iou(const FaceBox *a, const FaceBox *b)
{
    float inter_x1 = (a->x1 > b->x1) ? a->x1 : b->x1;
    float inter_y1 = (a->y1 > b->y1) ? a->y1 : b->y1;
    float inter_x2 = (a->x2 < b->x2) ? a->x2 : b->x2;
    float inter_y2 = (a->y2 < b->y2) ? a->y2 : b->y2;

    float iw = inter_x2 - inter_x1;
    float ih = inter_y2 - inter_y1;
    if (iw <= 0.0f || ih <= 0.0f) return 0.0f;

    float inter = iw * ih;
    float area_a = (a->x2 - a->x1) * (a->y2 - a->y1);
    float area_b = (b->x2 - b->x1) * (b->y2 - b->y1);

    /* 闁灝鍘ら梽銈夋祩 */
    if (area_a <= 0.0f || area_b <= 0.0f) return 0.0f;

    return inter / (area_a + area_b - inter);
}

/*
 * RFB-320 INT8 鏉堟挸鍤弽鐓庣础:
 *   scores[2][4420]: int8, Q(scale=0.00390625, zp=0)
 *     閳?score_f = scores_i8 * 0.00390625
 *   boxes[4][4420]:  int8, Q(scale=0.0060765594, zp=0)
 *     閳?box_f = boxes_i8 * 0.0060765594
 *     閳?(cx, cy, w, h) 瑜版帊绔撮崠鏍ф綏閺? *
 * 鐟欙絿鐖? x1=cx-w/2, y1=cy-h/2, x2=cx+w/2, y2=cy+h/2
 */
static int cmp_face_score_desc(const void *a, const void *b)
{
    float sa = ((const FaceBox *)a)->score;
    float sb = ((const FaceBox *)b)->score;
    if (sa > sb) return -1;
    if (sa < sb) return  1;
    return 0;
}

/* RFB-320 output quantization from generated network metadata. */
#define FACE_SCORE_SCALE  0.00390625f
#define FACE_BOX_SCALE    0.0060765594f

static int face_nms(const int8_t *scores, const int8_t *boxes_top4,
                     FaceBox *out, int max_out)
{
    /* 娴ｈ法鏁ら棃娆愨偓浣虹处閸愭煡浼╅崗?106KB 閺嶅牊瀛╅崙?*/
    FaceBox *candidates = nms_candidates;
    int *kept_flags = nms_kept_flags;
    int n_cand = 0;
    float max_face_score = 0.0f;
    int max_face_index = -1;

    for (int i = 0; i < FACE_MAX_CANDIDATES; i++) {
        /* scores shape is [4420][2], unsigned Q(scale=1/256, zp=-128). */
        float face_score = (float)(scores[i * 2 + 1] + 128) * FACE_SCORE_SCALE;
        if (face_score > max_face_score) {
            max_face_score = face_score;
            max_face_index = i;
        }
        if (face_score < FACE_CONF_THRESHOLD)
            continue;

        /* boxes shape is [4420][4], signed Q(scale, zp=-87). */
        float cx = (float)(boxes_top4[i * 4 + 0] + 87) * FACE_BOX_SCALE;
        float cy = (float)(boxes_top4[i * 4 + 1] + 87) * FACE_BOX_SCALE;
        float w  = (float)(boxes_top4[i * 4 + 2] + 87) * FACE_BOX_SCALE;
        float h  = (float)(boxes_top4[i * 4 + 3] + 87) * FACE_BOX_SCALE;

        float x1 = cx - w * 0.5f;
        float y1 = cy - h * 0.5f;
        float x2 = cx + w * 0.5f;
        float y2 = cy + h * 0.5f;

        /* 鐟佷礁澹€閸?[0,1] */
        if (x1 < 0.0f) x1 = 0.0f;
        if (y1 < 0.0f) y1 = 0.0f;
        if (x2 > 1.0f) x2 = 1.0f;
        if (y2 > 1.0f) y2 = 1.0f;

        /* 鏉╁洦鎶ら弮鐘虫櫏濡?*/
        if (w <= 0.0f || h <= 0.0f) continue;
        if (x2 <= x1 || y2 <= y1)   continue;

        candidates[n_cand].x1 = x1;
        candidates[n_cand].y1 = y1;
        candidates[n_cand].x2 = x2;
        candidates[n_cand].y2 = y2;
        candidates[n_cand].score = face_score;
        n_cand++;

        if (n_cand >= FACE_MAX_CANDIDATES) break;
    }

    printf("[人脸] 最高置信度=%.3f (anchor=%d), 阈值=%.2f\r\n",
           (double)max_face_score, max_face_index, (double)FACE_CONF_THRESHOLD);

    if (n_cand == 0) return 0;

    /* 缁楊兛绨╁? 閹稿鐤嗘穱鈥冲闂勫秴绨幒鎺戠碍 */
    qsort(candidates, n_cand, sizeof(FaceBox), cmp_face_score_desc);

    /* 缁楊兛绗佸? NMS 閳?閺嶅洩顔囨穱婵堟殌閻ㄥ嫭顢?*/
    int n_kept = 0;
    for (int i = 0; i < n_cand; i++) {
        int keep = 1;
        for (int j = 0; j < n_kept; j++) {
            int kj = kept_flags[j];
            if (face_iou(&candidates[i], &candidates[kj]) >= FACE_IOU_THRESHOLD) {
                keep = 0;
                break;
            }
        }
        if (keep) {
            kept_flags[n_kept++] = i;
            if (n_kept >= max_out) break;
        }
    }

    /* 缁楊剙娲撳? 鏉堟挸鍤紒鎾寸亯 */
    for (int i = 0; i < n_kept; i++) {
        out[i] = candidates[kept_flags[i]];
    }

    return n_kept;
}

/* ---- 娴滈缚鍔崠鍝勭厵鐟佷礁澹€ + 缂傗晜鏂?+ INT8 闁插繐瀵?---- */
/*
 * 娴?face_img (3xHxW int8 CHW, scale=0.0078125, zp=-1) 娑擃叀顥嗛崜顏冩眽閼寸灏崺?
 * 閻劍娓舵潻鎴﹀仸闁插洦鐗辩紓鈺傛杹閸?224x224, 閸氬本妞傞崣宥夊櫤閸栨牕鍩?float 閸愬秹鍣搁弬浼村櫤閸栨牕鍩?
 * 鐞涖劍鍎忓Ο鈥崇€烽幍鈧棁鈧惃?int8 Q(scale=0.007843138, zp=0)
 *
 * 閸欏秹鍣洪崠? fval = (i8_zp - (-1)) * 0.0078125 = (i8_zp + 1) * 0.0078125
 * 闁插秵鏌婇柌蹇撳: i8_expr = round(fval / 0.007843138) = round(fval * 127.5)
 */
static void quantize_face_to_expr(const int8_t *face_img,
                                  int img_w, int img_h,
                                  float x1, float y1, float x2, float y2,
                                  int8_t *expr_224)
{
    int expr_w = 224, expr_h = 224;
    int channels = 3;

    /* 閸嶅繒绀岄崸鎰垼: 瑜版帊绔撮崠?[0,1] 閳?閸ユ儳鍎氶崸鎰垼 [0, W/H-1] */
    int src_x1 = (int)(x1 * (float)(img_w - 1));
    int src_y1 = (int)(y1 * (float)(img_h - 1));
    int src_x2 = (int)(x2 * (float)(img_w - 1));
    int src_y2 = (int)(y2 * (float)(img_h - 1));

    int src_w = src_x2 - src_x1;
    int src_h = src_y2 - src_y1;

    if (src_w <= 0 || src_h <= 0) {
        /* 閺冪姵鏅ュ?閳?婵夘偊娴?*/
        memset(expr_224, 0, channels * expr_w * expr_h);
        return;
    }

    /*
     * 閺堚偓鏉╂垿鍋︾紓鈺傛杹: 鐎佃鐦℃稉顏嗘窗閺嶅洤鍎氱槐?(dx, dy), 閺勭姴鐨犻崶鐐寸爱閸ユ儳鍎氶崸鎰垼,
     * 閸欐牗娓舵潻鎴﹀仸閸嶅繒绀岄惃?int8 閸? 閸欏秹鍣洪崠鏍у煂 float, 閸愬秹鍣搁弬浼村櫤閸栨牕鍩岀悰銊﹀剰 int8
     *
     * 閸愬懎鐡ㄧ敮鍐ㄧ湰: CHW 閳?expr_224[c * 224*224 + dy * 224 + dx]
     */
    for (int dy = 0; dy < expr_h; dy++) {
        int sy = src_y1 + (dy * src_h) / expr_h;
        if (sy >= img_h) sy = img_h - 1;

        for (int dx = 0; dx < expr_w; dx++) {
            int sx = src_x1 + (dx * src_w) / expr_w;
            if (sx >= img_w) sx = img_w - 1;

            for (int c = 0; c < channels; c++) {
                int8_t i8_face = face_img[c * img_h * img_w + sy * img_w + sx];

                /* 閸欏秹鍣洪崠鏍у煂 float: fval = (i8_face + 1) * 0.0078125
                 * 閸愬秹鍣洪崠鏍у煂鐞涖劍鍎?int8: i8_expr = round(fval * 127.5)
                 * 閸氬牆鑻? i8_expr = round((i8_face + 1) * 0.0078125 * 127.5)
                 *             = round((i8_face + 1) * 0.99609375)
                 * 鏉╂垳鎶€:       閳?round((i8_face + 1) * 1.0)
                 *             = i8_face + 1  */
                int i8 = (int)i8_face + 1;
                if (i8 >  127) i8 =  127;
                if (i8 < -128) i8 = -128;

                expr_224[c * expr_h * expr_w + dy * expr_w + dx] = (int8_t)i8;
            }
        }
    }
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/* ---- I2C2 閸掓繂顫愰崠?(IMX335 閹藉嫬鍎氭径? ---- */
static void MX_I2C2_Init(void)
{
    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x30C0EDFF;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_10);
        for (volatile int i = 0; i < 2000000; i++);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    printf("ASSERT: %s:%lu\r\n", (char *)file, (unsigned long)line);
    while (1);
}
#endif
