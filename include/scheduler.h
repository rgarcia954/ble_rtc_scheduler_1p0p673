/**
 * @file scheduler.h
 * @brief Scheduler header file
 *
 * @copyright @parblock
 * Copyright (c) 2022 Semiconductor Components Industries, LLC (d/b/a
 * onsemi), All Rights Reserved
 *
 * This code is the property of onsemi and may not be redistributed
 * in any form without prior written permission from onsemi.
 * The terms of use and warranty for this code are covered by contractual
 * agreements between onsemi and the licensee.
 *
 * This is Reusable Code.
 * @endparblock
 */

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <stddef.h>
#include <stdint.h>

#include "swmTrace_api.h"
#include "montana.h"
#include "app.h"

#define DEBUG_SCHEDULER         (0)             /**< Set 1 to enable scheduler debug logs. */

#define SCHEDULER_TASK_MAX              (10)    /**< Maximum number of task that can be registered.  */
#define SCHEDULER_MIN_BURST_TIME        CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_1S)     /**< Minimum time that task
                                                                                             * can wait to run. */
#define SCHEDULER_MAX_BURST_TIME        CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_M(10))    /**< Maximum time that
                                                                                               * task can wait to run.
                                                                                               * */

/** Function pointer for scheduler task */
typedef void (*p_schedular_task_t)(void);

/**
 * @brief enum for scheduler task states
 *
 */
typedef enum
{
    TASK_BLOCKED = 0,                        /**< Default State for the Task. */
    TASK_READY,                              /**< Task will execute actual work when READY. */
    TASK_SUSPENDED                           /**< Task will be ignored by scheduler. */
} Scheduler_Task_State_t;

/**
 * @brief scheduler task
 *
 */
typedef struct scheduler_task_t
{
	p_schedular_task_t task_function;               /**< Function that gets called when task is READY. */
    uint32_t arrival_cycles;                /**< The period we want to put task to be READY (Number of RTC cycles) */
    uint32_t count_cycles;                /**< Counter, If it reaches the arrival_cycles, then the timer puts it into
                                           * READY state. */
    Scheduler_Task_State_t task_state;                  /**< The current state of the task. */
} scheduler_task;

/**
 * @brief enum for task creation return
 *
 */
typedef enum
{
    TASK_CREATE_ERR_NONE = 0,                           /**< Task created successfully. */
    TASK_CREATE_ERR_NULL_PTR,                       /**< Task creation error: NULL pointer. */
    TASK_CREATE_ERR_TIME_LIMIT,                     /**< Task creation error: Task Burst cycle is not within limits.
                                                       * */
	TASK_CREATE_ERR_COUNT_LIMIT,					/**< Task creation error: Maximum task limit reached. */
    TASK_CREATE_ERR_UNKNOWN							/**< Task creation error: Not known Error. */
} Task_Creation_t;

/**
 * @brief Create a new task for schedule.
 *
 * @param[in] p_schedular_task	Pointer to function for task to execute when TASK_READY
 * @param[in] arrival_cycles    Number of RTC cycles task will wait before being TASK_READY
 *
 * @note  By default created task will get set to TASK_BLOCKED.
 *
 * @return Task creation status whether creation of task was successful or not.
 */
Task_Creation_t Scheduler_Create_NewTask(p_schedular_task_t task, uint32_t arrival_cycles);

/**
 * @brief Update count cycle for a scheduled task.
 *
 * @param[in] wakeup_cycle_count	value to write to task count_cycle
 */
void Scheduler_Update_CountCycle(uint32_t wakeup_cycle_count);

/**
 * @brief Run task which are in TASK_READY state
 *
 */
void Scheduler_Run_ReadyTask(void);

/**
 * @brief Calculate duration after which it will require to wake up
 *
 * @return Number of cycles before next task will be ready to run
 */
uint64_t Scheduler_Calculate_SleepDuration(void);

/**
 * @brief Set/Update burst cycle for the scheduled task
 *
 * @param[in] scheduled_task_number	Number of given task in scheduler_task_queue
 * @param[in] arrival_cycle         Number of RTC cycles task will wait before
 *                                  being TASK_READY
 */
void Scheduler_Set_ArrivalCycle(uint8_t scheduled_task_number, uint32_t arrival_cycle);

/**
 * @brief Create a task/s for scheduler.
 */
void Scheduler_Create_Tasks(void);

/**
 * @brief Main part of scheduler.
 */
void Scheduler_Main(void);

#endif    /* INCLUDE_SCHEDULER_H_ */
