/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for myOLEDTasj */
osThreadId_t myOLEDTasjHandle;
const osThreadAttr_t myOLEDTasj_attributes = {
  .name = "myOLEDTasj",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for myKEYTask */
osThreadId_t myKEYTaskHandle;
const osThreadAttr_t myKEYTask_attributes = {
  .name = "myKEYTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for myTempTask */
osThreadId_t myTempTaskHandle;
const osThreadAttr_t myTempTask_attributes = {
  .name = "myTempTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for myEncoderTask */
osThreadId_t myEncoderTaskHandle;
const osThreadAttr_t myEncoderTask_attributes = {
  .name = "myEncoderTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for KEY1Queue */
osMessageQueueId_t KEY1QueueHandle;
const osMessageQueueAttr_t KEY1Queue_attributes = {
  .name = "KEY1Queue"
};
/* Definitions for KEY2Queue */
osMessageQueueId_t KEY2QueueHandle;
const osMessageQueueAttr_t KEY2Queue_attributes = {
  .name = "KEY2Queue"
};
/* Definitions for I2C1Mutex */
osMutexId_t I2C1MutexHandle;
const osMutexAttr_t I2C1Mutex_attributes = {
  .name = "I2C1Mutex"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartOLEDTask(void *argument);
void StartKEYTask(void *argument);
void StartTempTask(void *argument);
void StartEncoderTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of I2C1Mutex */
  I2C1MutexHandle = osMutexNew(&I2C1Mutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of KEY1Queue */
  KEY1QueueHandle = osMessageQueueNew (8, sizeof(uint16_t), &KEY1Queue_attributes);

  /* creation of KEY2Queue */
  KEY2QueueHandle = osMessageQueueNew (8, sizeof(uint16_t), &KEY2Queue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of myOLEDTasj */
  myOLEDTasjHandle = osThreadNew(StartOLEDTask, NULL, &myOLEDTasj_attributes);

  /* creation of myKEYTask */
  myKEYTaskHandle = osThreadNew(StartKEYTask, NULL, &myKEYTask_attributes);

  /* creation of myTempTask */
  myTempTaskHandle = osThreadNew(StartTempTask, NULL, &myTempTask_attributes);

  /* creation of myEncoderTask */
  myEncoderTaskHandle = osThreadNew(StartEncoderTask, NULL, &myEncoderTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
__weak void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartOLEDTask */
/**
* @brief Function implementing the myOLEDTasj thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartOLEDTask */
__weak void StartOLEDTask(void *argument)
{
  /* USER CODE BEGIN StartOLEDTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartOLEDTask */
}

/* USER CODE BEGIN Header_StartKEYTask */
/**
* @brief Function implementing the myKEYTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartKEYTask */
__weak void StartKEYTask(void *argument)
{
  /* USER CODE BEGIN StartKEYTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartKEYTask */
}

/* USER CODE BEGIN Header_StartTempTask */
/**
* @brief Function implementing the myTempTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTempTask */
__weak void StartTempTask(void *argument)
{
  /* USER CODE BEGIN StartTempTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTempTask */
}

/* USER CODE BEGIN Header_StartEncoderTask */
/**
* @brief Function implementing the myEncoderTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartEncoderTask */
__weak void StartEncoderTask(void *argument)
{
  /* USER CODE BEGIN StartEncoderTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartEncoderTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

