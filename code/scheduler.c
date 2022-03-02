/**
 * @file scheduler.c
 * @brief Scheduler source file
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
#include "scheduler.h"

static scheduler_task scheduler_task_queue[SCHEDULER_TASK_MAX];     /**< Task queue. */
static uint8_t total_scheduled_tasks = 0;                           /**< Number of the registered tasks. */

uint32_t calc_sleep_duration = 0;    /**< Calculated sleep duration in number of cycles */
uint32_t pre_sleep_duration = 0;    /**< Previous sleep duration in number of cycles */
uint32_t prog_sleep_duration = 0;    /**< Programmed sleep duration in number of cycles */

Task_Creation_t Scheduler_Create_NewTask(p_schedular_task_t task, uint32_t arrival_cycles)
{
    Task_Creation_t task_status = TASK_CREATE_ERR_NONE;

    if (scheduler_task_queue == NULL)
    {
        task_status = TASK_CREATE_ERR_NULL_PTR;
    }
    else if ((SCHEDULER_MIN_BURST_TIME > arrival_cycles) || (SCHEDULER_MAX_BURST_TIME < arrival_cycles))
    {
        task_status = TASK_CREATE_ERR_TIME_LIMIT;
    }
    else if (SCHEDULER_TASK_MAX <= total_scheduled_tasks)
    {
        task_status = TASK_CREATE_ERR_COUNT_LIMIT;
    }
    else if (task == NULL)
    {
        task_status = TASK_CREATE_ERR_NULL_PTR;
    }
    else
    {
        scheduler_task_queue[total_scheduled_tasks].task_function = task;
        scheduler_task_queue[total_scheduled_tasks].arrival_cycles = arrival_cycles;
        scheduler_task_queue[total_scheduled_tasks].task_state = TASK_BLOCKED;
        scheduler_task_queue[total_scheduled_tasks].count_cycles = 0;
        total_scheduled_tasks++;
    }

    return task_status;
}

void Scheduler_Update_CountCycle(uint32_t wakeup_cycle_count)
{
    /* Update count cycle for all task/s which are not SUSPENDED */
    for (uint8_t i = 0; i < total_scheduled_tasks; i++)
    {
        /* Ignore SUSPENDED tasks. */
        if (TASK_SUSPENDED != scheduler_task_queue[i].task_state)
        {
            scheduler_task_queue[i].count_cycles += wakeup_cycle_count;
        }
    }

    /* Update task to READY */
    for (uint8_t i = 0; i < total_scheduled_tasks; i++)
    {
        /* Ignore SUSPENDED tasks. */
        if (TASK_SUSPENDED != scheduler_task_queue[i].task_state)
        {
            /* Put it into READY state. */
            if (scheduler_task_queue[i].arrival_cycles <= scheduler_task_queue[i].count_cycles)
            {
                scheduler_task_queue[i].count_cycles = 0;
                scheduler_task_queue[i].task_state  = TASK_READY;
            }
        }
    }
}

void Scheduler_Run_ReadyTask(void)
{
    for (uint8_t i = 0; i < total_scheduled_tasks; i++)
    {
        /* If it is ready, then call it.*/
        if (TASK_READY == scheduler_task_queue[i].task_state)
        {
            if(scheduler_task_queue[i].task_function)
            {
            	scheduler_task_queue[i].task_function();
            }
            scheduler_task_queue[i].task_state = TASK_BLOCKED;
        }
    }
}

uint64_t Scheduler_Calculate_SleepDuration(void)
{
    /* Next wake up time should be burst_time - current cycle_count */
    uint64_t next_wakeup_time = SCHEDULER_MAX_BURST_TIME;
    uint64_t task_wakeup_time = SCHEDULER_MAX_BURST_TIME;

    for (uint8_t i = 0; i < total_scheduled_tasks; i++)
    {
        if (TASK_BLOCKED == scheduler_task_queue[i].task_state)
        {
            task_wakeup_time = scheduler_task_queue[i].arrival_cycles - scheduler_task_queue[i].count_cycles;

            if (task_wakeup_time <= next_wakeup_time)
            {
                next_wakeup_time = task_wakeup_time;
            }
        }
    }

    return next_wakeup_time;
}

void Scheduler_Set_ArrivalCycle(uint8_t scheduled_task_number, uint32_t arrival_cycle)
{
    scheduler_task_queue[scheduled_task_number].arrival_cycles = arrival_cycle;
}

void Scheduler_Create_Tasks(void)
{
    Scheduler_Create_NewTask(&Task0_BLEAdvControl, CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(BLE_ADV_ON_DURATION)));
    Scheduler_Create_NewTask(&Task1_Dummy, CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(TASK1_BURST_TIME_S)));
}

void Scheduler_Main(void)
{
    static uint8_t startup_flag = 1;

    /* Check startup once to setup initial count cycles for each
     * scheduled tasks */
    if (startup_flag)
    {
        Scheduler_Update_CountCycle(CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_1S));
        pre_sleep_duration = CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_1S);
        startup_flag = 0;
    }
    else
    {
        Scheduler_Update_CountCycle(calc_sleep_duration);
        pre_sleep_duration = prog_sleep_duration;
    }

    /* Run tasks which are in READY state */
    Scheduler_Run_ReadyTask();

    /* Calculate sleep time for next task need to execute  */
    calc_sleep_duration = Scheduler_Calculate_SleepDuration();

#if DEBUG_SCHEDULER
    swmLogInfo("Calculated sleep duration = %d millisec\n\r", (uint32_t)(calc_sleep_duration / 32.768));
    Sys_Delay(0.025 * SystemCoreClock);
#endif    /* if DEBUG_SCHEDULER */

    /* Re-configure RTC wakeup time before entering sleep */
    prog_sleep_duration = RTC_ALARM_Reconfig(calc_sleep_duration, pre_sleep_duration, true);

#if DEBUG_SCHEDULER
    swmLogInfo("Programmed sleep duration = %d millisec\n\r", (uint32_t)(prog_sleep_duration / 32.768));
    Sys_Delay(0.025 * SystemCoreClock);
#endif    /* if DEBUG_SCHEDULER */
}
