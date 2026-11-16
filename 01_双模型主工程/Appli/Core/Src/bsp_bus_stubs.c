/**
 * @file        bsp_bus_stubs.c
 * @brief       I2C 总线 stub 实现 (替换 991 的 bsp_bus.c)
 *              直接调用 HAL 库，不需要 ThreadX 或 bsp 抽象层
 */
#include "stm32n6xx_hal.h"

extern I2C_HandleTypeDef hi2c2;

static uint8_t i2c2_init_counter = 0;

int32_t bsp_i2c2_init(void)
{
    if (i2c2_init_counter == 0) {
        i2c2_init_counter++;
        /* I2C2 初始化由 main.c 中的 MX_I2C2_Init() 完成 */
    }
    return 0;
}

int32_t bsp_i2c2_deinit(void)
{
    if (i2c2_init_counter > 0) {
        i2c2_init_counter--;
        if (i2c2_init_counter == 0) {
            HAL_I2C_DeInit(&hi2c2);
        }
    }
    return 0;
}

int32_t bsp_i2c2_write_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length)
{
    if (HAL_I2C_Mem_Write(&hi2c2, address, reg, I2C_MEMADD_SIZE_16BIT, data, length, 1000) != HAL_OK) {
        return -1;
    }
    return 0;
}

int32_t bsp_i2c2_read_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length)
{
    if (HAL_I2C_Mem_Read(&hi2c2, address, reg, I2C_MEMADD_SIZE_16BIT, data, length, 1000) != HAL_OK) {
        return -1;
    }
    return 0;
}
