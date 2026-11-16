/**
 * @file    MX_DCMIPP_ClockConfig.c
 * @brief   DCMIPP/CSI 时钟配置 (覆盖 cmw_camera.c 中的 weak 实现)
 */
#include "stm32n6xx_hal.h"

HAL_StatusTypeDef MX_DCMIPP_ClockConfig(DCMIPP_HandleTypeDef *hdcmipp)
{
    (void)hdcmipp;
    RCC_PeriphCLKInitTypeDef rcc_periph_clk_init_struct = {0};

    rcc_periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_DCMIPP | RCC_PERIPHCLK_CSI;
    rcc_periph_clk_init_struct.DcmippClockSelection = RCC_DCMIPPCLKSOURCE_IC17;
    /* Match the 800 MHz ATK 991 AI project: DCMIPP=PLL2/3, CSI=PLL1/40. */
    rcc_periph_clk_init_struct.ICSelection[RCC_IC17].ClockSelection = RCC_ICCLKSOURCE_PLL2;
    rcc_periph_clk_init_struct.ICSelection[RCC_IC17].ClockDivider = 3;
    rcc_periph_clk_init_struct.ICSelection[RCC_IC18].ClockSelection = RCC_ICCLKSOURCE_PLL1;
    rcc_periph_clk_init_struct.ICSelection[RCC_IC18].ClockDivider = 40;
    if (HAL_RCCEx_PeriphCLKConfig(&rcc_periph_clk_init_struct) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}
