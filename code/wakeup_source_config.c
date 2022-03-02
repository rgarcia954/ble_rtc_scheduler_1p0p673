/**
 * @file wakeup_source_config.c
 * @brief Wakeup source configuration file
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

#include "wakeup_source_config.h"
#include "sensor.h"
#include "app.h"

void Wakeup_Source_Config(void)
{
    /* Configure and enable RTC wakeup source */
    RTC_ALARM_Init();

    /* Configure and enable GPIO wakeup source */
    GPIO_Wakeup_Init();

    /* Clear GPIO1 sticky wakeup flag */
    WAKEUP_GPIO1_FLAG_CLEAR();

    /* Disable NVIC GPIO1 interrupt if the GPIO1 is used as wakeup source
     * GPIO1 NVIC IRQ is recommended to disable if GPIO1 is used as wakeup
     * source. WAKEUP_IRQn is used to capture GPIO1 wakeup event */
    NVIC_DisableIRQ(GPIO1_IRQn);

    /* Clear NVIC Wakeup interrupt IRQ */
    NVIC_ClearPendingIRQ(WAKEUP_IRQn);

    /* Enable the Wakeup interrupt */
    NVIC_EnableIRQ(WAKEUP_IRQn);
}

/* Configure GPIO inputs and interrupts */
void GPIO_Wakeup_Init()
{
    SYS_GPIO_CONFIG(GPIO_WAKEUP_PIN, (GPIO_MODE_DISABLE | GPIO_LPF_DISABLE |
                                      GPIO_WEAK_PULL_UP  | GPIO_6X_DRIVE));
}

/**
 * @brief       Configure and enable RTC ALARM event
 * @param[in]	timer_counter Start value(number of cycles)
 *              for RTC timer counter
 * @assumptions The following are pre-defined;
 *              RTC_CLK_SRC: RTC clock source
 */
void RTC_ALARM_Init(void)
{
    /* Configure and enable clock source for RTC */
    RTC_ClockSource_Init();

    /* Configure RTC with timer_counter-1 as start value*/
    ACS->RTC_CTRL = RTC_DISABLE;
    ACS->RTC_CTRL = RTC_RESET;
    ACS->RTC_CFG = 0xDEADBEEF;
    ACS->RTC_CTRL = RTC_ENABLE | RTC_CLK_SRC | RTC_ALARM_ZERO;

    /* Clear sticky wakeup RTC alarm flag */
    WAKEUP_RTC_ALARM_FLAG_CLEAR();
}

/**
 * @brief Configure and enable clock source for RTC
 */
void RTC_ClockSource_Init(void)
{
    if (RTC_CLK_SRC == RTC_CLK_SRC_RC_OSC)
    {
        /* Enable RC32k without changing any other register bits */
        ACS->RCOSC_CTRL |= RC32_OSC_ENABLE;
    }
    if (RTC_CLK_SRC == RTC_CLK_SRC_GPIO0)
    {
        /* Configure GPIO0 as an input pin */
        SYS_GPIO_CONFIG(GPIO0, GPIO_MODE_INPUT);
    }
    if (RTC_CLK_SRC == RTC_CLK_SRC_GPIO1)
    {
        /* Configure GPIO1 as an input pin */
        SYS_GPIO_CONFIG(GPIO1, GPIO_MODE_INPUT);
    }
}

/**
 * @brief Read current RTC timer counter
 * @return current RTC timer counter value
 */
static uint32_t RTC_Timer_Counter_Read(void)
{
    uint32_t rtc_timer_count;

    /* Read current rtc timer counter value */
    rtc_timer_count = ACS->RTC_COUNT;

    /* Read back and if different read back again
     * (data can be corrupted as rtc_clock and sysclk are asynchronous) */
    if (rtc_timer_count != ACS->RTC_COUNT)
    {
        rtc_timer_count = ACS->RTC_COUNT;
    }
    return (rtc_timer_count);
}

/**
 * @brief Re-configure start value for the RTC timer counter
 *        and calculate total elapsed RTC cycles
 * @param [in] timer_counter Start value(number of cycles)
 *             for RTC timer counter
 * @param [in] pre_timer_counter previously programmed
 *             timer_counter start value
 * @param [in] prog_relative_timer_count if enabled it will program relative
 *             timer_counter value to compensate lost cycles between last
 *             wake up and now else it will program timer_counter value in arg1
 * @return programmed value of RTC_CFG register
 */
uint32_t RTC_ALARM_Reconfig(uint32_t timer_counter, uint32_t pre_timer_counter,
                            bool prog_relative_timer_count)
{
    static uint8_t startup_check = 0;
    uint32_t rtc_config_val = 0;

    /* NVIC set enable registers */
    uint32_t nvic_set_enable[2];

    /* Mask interrupts */
    __disable_irq();

    /* Backup NVIC set enable registers so that we can restore it later */
    nvic_set_enable[0] = NVIC->ISER[0];
    nvic_set_enable[1] = NVIC->ISER[1];

    /* Set NVIC clear enable registers to clear interrupts */
    NVIC->ICER[0] = 0xFFFFFFFFU;
    NVIC->ICER[1] = 0xFFFFFFFFU;

    /* Set wanted IRQ only (GPIO3) */
    NVIC_EnableIRQ(GPIO3_IRQn);

    /* Configure GPIO8 as standby clock */
    SYS_GPIO_CONFIG(8, GPIO_2X_DRIVE | GPIO_LPF_DISABLE | GPIO_NO_PULL | NS_CANNOT_USE_GPIO | GPIO_MODE_STANDBYCLK);

    /* Configure GPIO3 interrupt line to rising edge of GPIO8(standby clock) */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_NONE | GPIO_SRC_GPIO_8,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_RISING_EDGE | GPIO_SRC_GPIO_8,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    /* Wait for rising edge of RTC_CLOCK */
    __WFI();

    /* Clear the pending GPIO3 IRQ */
    NVIC_ClearPendingIRQ(GPIO3_IRQn);

    /* Read RTC timer counter */
    uint32_t rtc_counter = RTC_Timer_Counter_Read();

    /* Configure RTC timer counter with timeout cycles */
    if (prog_relative_timer_count == false)
    {
        rtc_config_val = timer_counter - 1;
        ACS->RTC_CFG = rtc_config_val;
    }
    else
    {
        rtc_config_val = timer_counter - 1 - ((0xDEADBEEF) - rtc_counter);
        ACS->RTC_CFG = rtc_config_val;
    }

    /* Configure GPIO3 interrupt line to falling edge of GPIO8(standby clock) */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_FALLING_EDGE | GPIO_SRC_GPIO_8,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    /* Wait for falling edge of RTC_CLOCK */
    __WFI();

    /* Reset RTC to load new timer counter */
    ACS->RTC_CTRL = (ACS->RTC_CTRL & ~ACS_RTC_CTRL_ALARM_CFG_Mask) | RTC_ALARM_DISABLE;
    ACS->RTC_CTRL |= RTC_RESET;
    ACS->RTC_CTRL |= RTC_FORCE_CLOCK;
    ACS->RTC_CTRL = (ACS->RTC_CTRL & ~ACS_RTC_CTRL_ALARM_CFG_Mask) | RTC_ALARM_ZERO;

    /* Clear sticky wakeup RTC alarm flag */
    WAKEUP_RTC_ALARM_FLAG_CLEAR();

    /* Set RTC preload timer counter to DEADBEEF for next wake up */
    ACS->RTC_CFG = 0xDEADBEEF;

    /* Clear the pending GPIO3_IRQ */
    NVIC_ClearPendingIRQ(GPIO3_IRQn);

    /* Disable GPIO3 interrupt line */
    Sys_GPIO_IntConfig(3, NS_CANNOT_ACCESS_GPIO_INT | GPIO_DEBOUNCE_DISABLE | GPIO_EVENT_NONE,
                       GPIO_DEBOUNCE_SLOWCLK_DIV32, 0);

    /* Reset GPIO8 */
    SYS_GPIO_CONFIG(8, GPIO_2X_DRIVE | GPIO_LPF_DISABLE | GPIO_WEAK_PULL_UP | NS_CANNOT_USE_GPIO | GPIO_MODE_DISABLE);

    /* Restore NVIC set enable register */
    NVIC->ISER[0] = nvic_set_enable[0];
    NVIC->ISER[1] = nvic_set_enable[1];

    /* Unmask interrupt */
    __enable_irq();

    /* Increment total RTC cycles count as necessary */
    if (startup_check == 0)
    {
        total_RTC_cycles += (0xDEADBEEF - rtc_counter);
        startup_check = 1;
    }
    else
    {
        if (rtc_counter == 0)
        {
            total_RTC_cycles += pre_timer_counter;
        }
        else
        {
            total_RTC_cycles += (pre_timer_counter + (0xDEADBEEF - rtc_counter) + 1);
        }
    }

    return rtc_config_val;
}
