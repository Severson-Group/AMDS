#include "drv_i2c.h"
#include "platform.h"
#include "defines.h"

static void MX_I2C2_Init(void);

static I2C_HandleTypeDef hi2c2;

void drv_i2c_init(void)
{
	MX_I2C2_Init();
}

static void MX_I2C2_Init(void)
{
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x00C0EAFF;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
    PANIC;
  }

  // Configure Analog filter
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK) {
    PANIC;
  }

  // Configure Digital filter
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK) {
    PANIC;
  }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef* i2cHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  if (i2cHandle->Instance == I2C2) {
    __HAL_RCC_GPIOF_CLK_ENABLE();
    // I2C2 GPIO Configuration
    // PF0     ------> I2C2_SDA
    // PF1     ------> I2C2_SCL
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    // I2C2 clock enable
    __HAL_RCC_I2C2_CLK_ENABLE();
  }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{
  if (i2cHandle->Instance == I2C2) {
    // Peripheral clock disable */
    __HAL_RCC_I2C2_CLK_DISABLE();
  
    // I2C2 GPIO Configuration
    // PF0     ------> I2C2_SDA
    // PF1     ------> I2C2_SCL
    HAL_GPIO_DeInit(GPIOF, GPIO_PIN_0|GPIO_PIN_1);
  }
}

