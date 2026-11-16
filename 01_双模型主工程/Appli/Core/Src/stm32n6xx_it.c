/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32n6xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32n6xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */
typedef struct
{
  const char *name;
  uint32_t exc_return;
  uint32_t ipsr;
  uint32_t msp;
  uint32_t psp;
  uint32_t raw_sp;
  uint32_t fpccr;
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xpsr;
  uint32_t stage;
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t dfsr;
  uint32_t afsr;
  uint32_t mmfar;
  uint32_t bfar;
  uint32_t sfsr;
  uint32_t sfar;
} FaultSnapshot;

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FAULT_UART_SPIN_LIMIT  100000U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static volatile FaultSnapshot fault_snapshot;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static void Fault_UartPutc(char value);
static void Fault_UartWrite(const char *text);
static void Fault_UartHex32(uint32_t value);
static void Fault_ReportStacked(const char *name, const uint32_t *stacked,
                                uint32_t exc_return) __attribute__((noreturn));
static void Fault_Report(const char *name) __attribute__((noreturn));
void HardFault_HandlerC(const uint32_t *stacked, uint32_t exc_return) __attribute__((noreturn));

extern volatile uint32_t g_fault_stage;

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void Fault_UartPutc(char value)
{
  uint32_t spins = FAULT_UART_SPIN_LIMIT;

  while (((USART1->ISR & USART_ISR_TXE_TXFNF) == 0U) && (spins > 0U))
  {
    spins--;
  }

  if (spins > 0U)
  {
    USART1->TDR = (uint8_t)value;
  }
}

static void Fault_UartWrite(const char *text)
{
  while (*text != '\0')
  {
    Fault_UartPutc(*text++);
  }
}

static void Fault_UartHex32(uint32_t value)
{
  static const char digits[] = "0123456789ABCDEF";

  for (int32_t shift = 28; shift >= 0; shift -= 4)
  {
    Fault_UartPutc(digits[(value >> (uint32_t)shift) & 0x0FU]);
  }
}

static void Fault_ReportStacked(const char *name, const uint32_t *stacked, uint32_t exc_return)
{
  __disable_irq();

  fault_snapshot.name = name;
  fault_snapshot.exc_return = exc_return;
  fault_snapshot.ipsr = __get_IPSR();
  fault_snapshot.msp = __get_MSP();
  fault_snapshot.psp = __get_PSP();
  fault_snapshot.raw_sp = (uint32_t)stacked;
  fault_snapshot.fpccr = FPU->FPCCR;
  fault_snapshot.stage = g_fault_stage;
  if (stacked != NULL)
  {
    fault_snapshot.r0 = stacked[0];
    fault_snapshot.r1 = stacked[1];
    fault_snapshot.r2 = stacked[2];
    fault_snapshot.r3 = stacked[3];
    fault_snapshot.r12 = stacked[4];
    fault_snapshot.lr = stacked[5];
    fault_snapshot.pc = stacked[6];
    fault_snapshot.xpsr = stacked[7];
  }
  fault_snapshot.cfsr = SCB->CFSR;
  fault_snapshot.hfsr = SCB->HFSR;
  fault_snapshot.dfsr = SCB->DFSR;
  fault_snapshot.afsr = SCB->AFSR;
  fault_snapshot.mmfar = SCB->MMFAR;
  fault_snapshot.bfar = SCB->BFAR;
  fault_snapshot.sfsr = SCB->SFSR;
  fault_snapshot.sfar = SCB->SFAR;

  Fault_UartWrite("\r\n");
  Fault_UartWrite(name);
  Fault_UartWrite("\r\nFAULT-STAGE=");
  Fault_UartHex32(fault_snapshot.stage);
  Fault_UartWrite(" EXC_RETURN=");
  Fault_UartHex32(fault_snapshot.exc_return);
  Fault_UartWrite(" IPSR=");
  Fault_UartHex32(fault_snapshot.ipsr);
  Fault_UartWrite("\r\nFAULT-RAWSP=");
  Fault_UartHex32(fault_snapshot.raw_sp);
  Fault_UartWrite(" FPCCR=");
  Fault_UartHex32(fault_snapshot.fpccr);
  if (stacked != NULL)
  {
    Fault_UartWrite("\r\nBASIC-PC=");
    Fault_UartHex32(stacked[6]);
    Fault_UartWrite(" LR=");
    Fault_UartHex32(stacked[5]);
    Fault_UartWrite(" xPSR=");
    Fault_UartHex32(stacked[7]);
    Fault_UartWrite("\r\nEXT-PC=");
    Fault_UartHex32(stacked[24]);
    Fault_UartWrite(" LR=");
    Fault_UartHex32(stacked[23]);
    Fault_UartWrite(" xPSR=");
    Fault_UartHex32(stacked[25]);
    Fault_UartWrite("\r\nFAULT-STACK=");
    for (uint32_t index = 0U; index < 32U; index++)
    {
      if ((index != 0U) && ((index & 7U) == 0U))
      {
        Fault_UartWrite("\r\n            ");
      }
      Fault_UartHex32(stacked[index]);
      Fault_UartPutc(' ');
    }
  }
  Fault_UartWrite("\r\nR0=");
  Fault_UartHex32(fault_snapshot.r0);
  Fault_UartWrite(" R1=");
  Fault_UartHex32(fault_snapshot.r1);
  Fault_UartWrite(" R2=");
  Fault_UartHex32(fault_snapshot.r2);
  Fault_UartWrite(" R3=");
  Fault_UartHex32(fault_snapshot.r3);
  Fault_UartWrite(" R12=");
  Fault_UartHex32(fault_snapshot.r12);
  Fault_UartWrite("\r\nMSP=");
  Fault_UartHex32(fault_snapshot.msp);
  Fault_UartWrite(" PSP=");
  Fault_UartHex32(fault_snapshot.psp);
  Fault_UartWrite("\r\nCFSR=");
  Fault_UartHex32(fault_snapshot.cfsr);
  Fault_UartWrite(" HFSR=");
  Fault_UartHex32(fault_snapshot.hfsr);
  Fault_UartWrite(" DFSR=");
  Fault_UartHex32(fault_snapshot.dfsr);
  Fault_UartWrite(" AFSR=");
  Fault_UartHex32(fault_snapshot.afsr);
  Fault_UartWrite("\r\nMMFAR=");
  Fault_UartHex32(fault_snapshot.mmfar);
  Fault_UartWrite(" BFAR=");
  Fault_UartHex32(fault_snapshot.bfar);
  Fault_UartWrite(" SFSR=");
  Fault_UartHex32(fault_snapshot.sfsr);
  Fault_UartWrite(" SFAR=");
  Fault_UartHex32(fault_snapshot.sfar);
  Fault_UartWrite("\r\n");

  __DSB();
  __ISB();
  while (1)
  {
    __WFI();
  }
}

static void Fault_Report(const char *name)
{
  Fault_ReportStacked(name, NULL, 0U);
}

void HardFault_HandlerC(const uint32_t *stacked, uint32_t exc_return)
{
  Fault_ReportStacked("[FAULT] HardFault", stacked, exc_return);
}

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile(
      "tst lr, #4\n"
      "ite eq\n"
      "mrseq r0, msp\n"
      "mrsne r0, psp\n"
      "mov r1, lr\n"
      "b HardFault_HandlerC\n");
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
  Fault_Report("[FAULT] MemManage");
  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
  Fault_Report("[FAULT] BusFault");
  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
  Fault_Report("[FAULT] UsageFault");
  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Secure fault.
  */
void SecureFault_Handler(void)
{
  /* USER CODE BEGIN SecureFault_IRQn 0 */
  Fault_Report("[FAULT] SecureFault");
  /* USER CODE END SecureFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_SecureFault_IRQn 0 */
    /* USER CODE END W1_SecureFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32N6xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32n6xx.s).                    */
/******************************************************************************/

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
