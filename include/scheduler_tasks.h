/**
 * @file scheduler_tasks.h
 * @brief Task scheduler header file
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

#ifndef INCLUDE_SCHEDULER_TASKS_H_
#define INCLUDE_SCHEDULER_TASKS_H_

/**
 * @brief enum for task numbers
 *
 */
typedef enum
{
    TASK_0 = 0,
    TASK_1,
    TASK_2
} Task_Numbers_t;

#define TASK1_BURST_TIME_S 30

void Task0_BLEAdvControl(void);

void Task1_Dummy(void);

void Task2_Dummy(void);

#endif    /* INCLUDE_SCHEDULER_TASKS_H_ */
