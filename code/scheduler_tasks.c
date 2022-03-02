/**
 * @file scheduler_tasks.c
 * @brief Scheduler tasks source file
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

#include "scheduler_tasks.h"
#include "scheduler.h"

void Task0_BLEAdvControl(void)
{
	/* Set TASK0 GPIO Low at the beginning of Task execution */
	Sys_GPIO_Set_Low(TASK0_RUN_ACTIVITY_GPIO);

    /* flag to control BLE advertisement */
    static bool enable_ble_adv = false;

    /* Enable or disable BLE advertisement */
    ControlBLEAdvActivity(enable_ble_adv);

    if (enable_ble_adv)
    {
        /* Update arrival cycle for Task 0 to wake up system at BLE_ADV_ON_DURATION next time */
        Scheduler_Set_ArrivalCycle(TASK_0, CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(BLE_ADV_ON_DURATION)));
        enable_ble_adv = false;
    }
    else
    {
        /* Update arrival cycle for Task 0 to wake up system at BLE_ADV_OFF_DURATION next time */
        Scheduler_Set_ArrivalCycle(TASK_0, CONVERT_MS_TO_32K_CYCLES(RTC_SLEEP_TIME_S(BLE_ADV_OFF_DURATION)));
        enable_ble_adv = true;
    }

	/* Set TASK0 GPIO High at the end of Task execution */
	Sys_GPIO_Set_High(TASK0_RUN_ACTIVITY_GPIO);
}

void Task1_Dummy(void)
{
	/* Set TASK1 GPIO Low at the beginning of Task execution */
    Sys_GPIO_Set_Low(TASK1_RUN_ACTIVITY_GPIO);

    /* Perform Task here */

	/* Set TASK1 GPIO High at the end of Task execution */
    Sys_GPIO_Set_High(TASK1_RUN_ACTIVITY_GPIO);
}

void Task2_Dummy(void)
{
	/* Set TASK2 GPIO Low at the beginning of Task execution */
    Sys_GPIO_Set_Low(TASK2_RUN_ACTIVITY_GPIO);

    /* Perform Task here */

	/* Set TASK2 GPIO High at the end of Task execution */
    Sys_GPIO_Set_High(TASK2_RUN_ACTIVITY_GPIO);
}
