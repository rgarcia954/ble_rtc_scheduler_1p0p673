/**
 * @file app_init.c
 * @brief Source file for application initialization
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

#include <app.h>

#define XTAL32K_CTRIM_21P6PF    ((uint32_t)(0x36U << ACS_XTAL32K_CTRL_CLOAD_TRIM_Pos))
#define XTAL32K_CTRIM_23P2PF    ((uint32_t)(0x3AU << ACS_XTAL32K_CTRL_CLOAD_TRIM_Pos))

static uint32_t traceOptions[] = {
    SWM_LOG_LEVEL_INFO,                 /* In all cases log info messages */
    SWM_UART_RX_PIN | UART_RX_GPIO,     /* Set RX pin for cases when using UART */
    SWM_UART_TX_PIN | UART_TX_GPIO,     /* Set TX pin for cases when using UART */
    SWM_UART_RX_ENABLE,                 /* Enable the UART Rx Interrupts */
    SWM_UART_BAUD_RATE | UART_BAUD      /* Set Baud rate */
};

extern sleep_mode_cfg app_sleep_mode_cfg;

struct timeCode
{
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    uint16_t ms;
};

struct timeCode current_time;    /** To print current time based on RTC cycles */

void DeviceInit(void)
{
    /* Hold application here if recovery GPIO is held low during boot.
     * This makes it easier for the debugger to connect and reprogram the device. */
    SYS_GPIO_CONFIG(DEBUG_CATCH_GPIO, (GPIO_MODE_GPIO_IN | GPIO_LPF_DISABLE |
                                       GPIO_WEAK_PULL_UP  | GPIO_6X_DRIVE));

    while ((Sys_GPIO_Read(DEBUG_CATCH_GPIO)) == 0)
    {
        SYS_WATCHDOG_REFRESH();
    }

    /* Load default regulator trim values. */
    uint32_t trim_error __attribute__ ((unused)) = SYS_TRIM_LOAD_DEFAULT();

    /* Set all the GPIOs to a known state to minimize the leakage current from GPIO pins */
    for (uint8_t i = 0; i < GPIO_PAD_COUNT; i++)
    {
        SYS_GPIO_CONFIG(i, (GPIO_MODE_DISABLE | GPIO_LPF_DISABLE |
                            GPIO_STRONG_PULL_UP | GPIO_2X_DRIVE));
    }

    /* Calibrate the board */
#if ((CALIB_RECORD == SUPPLEMENTAL_CALIB) || (CALIB_RECORD == MANU_CALIB))
    if (Load_Trim_Values() !=
        VOLTAGES_CALIB_NO_ERROR)
    {
        /* Hold here to notify error(s) in voltage calibrations */
        while (true)
        {
            SYS_WATCHDOG_REFRESH();
        }
    }
#elif (CALIB_RECORD == USER_CALIB)
    if (Calculate_Trim_Values_And_Calibrate() !=
        VOLTAGES_CALIB_NO_ERROR)
    {
        /* Hold here to notify error(s) in voltage calibrations */
        while (true)
        {
            SYS_WATCHDOG_REFRESH();
        }
    }
#endif    /* CALIB_RECORD */

    /* Set ICH_TRIM for optimum RF performance */
    ACS->VCC_CTRL &= ~(ACS_VCC_CTRL_ICH_TRIM_Mask);
    ACS->VCC_CTRL |= ((uint32_t)(0x5U << ACS_VCC_CTRL_ICH_TRIM_Pos));

    /* Enable/disable buck converter */
    ACS->VCC_CTRL &= ~(VCC_BUCK);
    ACS->VCC_CTRL |= VCC_BUCK_LDO_CTRL;

    /* Configure and initialize system clock */
    App_Clock_Config();

#if DEBUG_SLEEP_GPIO

    /* Configure GPIOs */
    App_GPIO_Config();

    /* Clear POWER_MODE_GPIO to indicate wakeup */
    Sys_GPIO_Set_Low(POWER_MODE_GPIO);
#endif    /* if DEBUG_SLEEP_GPIO */

    /* Set radio output power of RF */
    int setTXPowerStatus = Sys_RFFE_SetTXPower(DEF_TX_POWER, LSAD_TXPWR_DEF, 0);
    tx_power_level_dbm = Sys_RFFE_GetTXPower(LSAD_TXPWR_DEF);

    /* If the actual set TX power is not within the accepted range
     * (i.e., the desired value +/- 1 dBm), hold the application here and prevent
     * it from executing further. */
    if ((tx_power_level_dbm < DEF_TX_POWER - 1) ||
        (tx_power_level_dbm > DEF_TX_POWER + 1))
    {
        while (true)
        {
            SYS_WATCHDOG_REFRESH();
        }
    }

    /* To demonstrate low power consumption, VCC_TARGET is set to 1.10V in calibration.h.
     * The optimal VCC_TARGET to achieve 0dBm in Sys_RFFE_SetTXPower() is 1.12V. So, the
     * function will return the status ERRNO_RFFE_INSUFFICIENTVCC_ERROR indicating the
     * VCC_TARGET may not be enough to reach 0dBm. So, we ignore this type of error here. */
    if (setTXPowerStatus != ERRNO_NO_ERROR &&
        setTXPowerStatus != ERRNO_RFFE_VCC_INSUFFICIENT)
    {
        while (1); /* Wait for watchdog reset! */
    }

#ifdef VOLTAGES_CALIB_VERIFY

    /* Hold here to verify calibrated voltages */
    while (true)
    {
        SYS_WATCHDOG_REFRESH();
    }
#endif    /* ifdef VOLTAGES_CALIB_VERIFY */

    /* Disable Sensor and CryptoCell power to achieve lowest power in Deep Sleep mode*/
#if SENSOR_POWER_DISABLE
    Sys_Sensor_Disable();
#endif    /* if SENSOR_POWER_DISABLE */

#if CC312AO_POWER_DISABLE
    Sys_Power_CC312AO_Disable();
#endif    /* if CC312AO_POWER_DISABLE */

#if POWER_DOWN_FPU

    /* Power Down FPU */
    Power_Down_FPU();
#endif    /* if POWER_DOWN_FPU */

#if POWER_DOWN_DBG

    /* Power Down DBG */
    Power_Down_Debug();
#endif    /* if POWER_DOWN_DBG */

#if VDDIF_POWER_DOWN

    /* Disable VDDIF */
    ACS->VDDIF_CTRL &= ~VDDIF_ENABLE;
#else    /* if VDDIF_POWER_DOWN */

    /* Enable VDDIF */
    ACS->VDDIF_CTRL |= VDDIF_ENABLE;
#endif    /* if VDDIF_POWER_DOWN */

    /* Configure the wakeup source */
    Wakeup_Source_Config();

    /* Sleep Initialization for Power Mode */
    App_Sleep_Initialization();

    /* Configure Baseband Controller Interface */
    BBIF->CTRL = (BB_CLK_ENABLE | BBCLK_DIVIDER_8);

    /* Set BB timer not reset bit */
    ACS->BB_TIMER_CTRL = BB_CLK_PRESCALE_1 | BB_TIMER_NRESET;

    /* Clear reset flags */
    RESET->DIG_STATUS = (uint32_t)0x1F00;
    ACS->RESET_STATUS = (uint32_t)0x3FF;

    /* Initialize trace library */
    swmTrace_init(traceOptions, 5);
}

void AppMsgHandlersInit(void)
{
    /* BLE Database setup handler */
    MsgHandler_Add(GAPM_CMP_EVT, BLE_ConfigHandler);
    MsgHandler_Add(GAPM_PROFILE_ADDED_IND, BLE_ConfigHandler);
    MsgHandler_Add(GATTM_ADD_SVC_RSP, BLE_ConfigHandler);

    /* BLE Activity handler (responsible for air operations) */
    MsgHandler_Add(GAPM_CMP_EVT, BLE_ActivityHandler);
    MsgHandler_Add(GAPM_ACTIVITY_CREATED_IND, BLE_ActivityHandler);
    MsgHandler_Add(GAPM_ACTIVITY_STOPPED_IND, BLE_ActivityHandler);

    /* Connection handler */
    MsgHandler_Add(GAPM_CMP_EVT, BLE_ConnectionHandler);
    MsgHandler_Add(GAPC_CONNECTION_REQ_IND, BLE_ConnectionHandler);
    MsgHandler_Add(GAPC_DISCONNECT_IND, BLE_ConnectionHandler);
    MsgHandler_Add(GAPM_ADDR_SOLVED_IND, BLE_ConnectionHandler);
    MsgHandler_Add(GAPC_GET_DEV_INFO_REQ_IND, BLE_ConnectionHandler);
    MsgHandler_Add(GAPC_PARAM_UPDATE_REQ_IND, BLE_ConnectionHandler);

    /* Pairing / bonding  handler */
    MsgHandler_Add(GAPC_BOND_REQ_IND, BLE_PairingHandler);
    MsgHandler_Add(GAPC_BOND_IND, BLE_PairingHandler);
    MsgHandler_Add(GAPC_ENCRYPT_REQ_IND, BLE_PairingHandler);
    MsgHandler_Add(GAPC_ENCRYPT_IND, BLE_PairingHandler);
}

void CustomServiceServerInit(void)
{
    CUSTOMSS_Initialize();
    CUSTOMSS_NotifyOnTimeout(TIMER_SETTING_S(10));
}

void IRQPriorityInit(void)
{
    uint8_t interrupt;

    /* Iterate through all external interrupts excluding WAKEUP_IRQn */
    for (interrupt = RTC_ALARM_IRQn; interrupt <= NVIC_LAST_VECTOR; ++interrupt)
    {
        /* If the interrupt is non-BLE, set priority to 1 (lower than
         * the default priority of 0). This ensures BLE stability. */
        if (interrupt < BLE_SW_IRQn || interrupt > BLE_ERROR_IRQn)
        {
            NVIC_SetPriority(interrupt, 1);
        }
    }
}

void BLEStackInit(void)
{
    uint8_t param_ptr;
    BLE_Initialize(&param_ptr);

    /* BLE_Initialize() initialized a number of trim registers
     * using default values from the BLE stack,
     * SYS_TRIM_LOAD_CUSTOM() ensures custom trim values are used. */
    uint32_t custom_trim_error __attribute__ ((unused)) = SYS_TRIM_LOAD_CUSTOM();

    ke_task_create(TASK_APP, MsgHandler_GetTaskAppDesc());
    Device_BLE_Public_Address_Read((uint32_t)APP_BLE_PUBLIC_ADDR_LOC);

    IRQPriorityInit();
}

void DisableAppInterrupts(void)
{
    Sys_NVIC_DisableAllInt();
    Sys_NVIC_ClearAllPendingInt();
    __set_PRIMASK(PRIMASK_DISABLE_INTERRUPTS);
    __set_FAULTMASK(FAULTMASK_DISABLE_INTERRUPTS);
}

void EnableAppInterrupts(void)
{
    NVIC_ClearPendingIRQ(BLE_HSLOT_IRQn);
    NVIC_ClearPendingIRQ(BLE_SLP_IRQn);
    NVIC_ClearPendingIRQ(BLE_FIFO_IRQn);
    NVIC_ClearPendingIRQ(BLE_CRYPT_IRQn);
    NVIC_ClearPendingIRQ(BLE_ERROR_IRQn);
    NVIC_ClearPendingIRQ(BLE_TIMESTAMP_TGT1_IRQn);
    NVIC_ClearPendingIRQ(BLE_FINETGT_IRQn);
    NVIC_ClearPendingIRQ(BLE_TIMESTAMP_TGT2_IRQn);
    NVIC_ClearPendingIRQ(BLE_SW_IRQn);

    NVIC_EnableIRQ(BLE_HSLOT_IRQn);
    NVIC_EnableIRQ(BLE_SLP_IRQn);
    NVIC_EnableIRQ(BLE_FIFO_IRQn);
    NVIC_EnableIRQ(BLE_CRYPT_IRQn);
    NVIC_EnableIRQ(BLE_ERROR_IRQn);
    NVIC_EnableIRQ(BLE_TIMESTAMP_TGT1_IRQn);
    NVIC_EnableIRQ(BLE_FINETGT_IRQn);
    NVIC_EnableIRQ(BLE_TIMESTAMP_TGT2_IRQn);
    NVIC_EnableIRQ(BLE_SW_IRQn);

    __set_FAULTMASK(FAULTMASK_ENABLE_INTERRUPTS);
    __set_PRIMASK(PRIMASK_ENABLE_INTERRUPTS);
}

/**
 * @brief Initializes System Clock
 */
void App_Clock_Config(void)
{
    uint8_t systemclk_div;

    /* Set the system clock prescale to output value defined in
     * SYSTEM_CLK */
    systemclk_div = (RFCLK_BASE_FREQ / SYSTEM_CLK);

    /* Boundary condition check for the system clock division */
    /* If the prescale is greater than CK_DIV_1_6_PRESCALE_7_BYTE set it to
     * CK_DIV_1_6_PRESCALE_7_BYTE to avoid system clock being an unknown value */
    if (systemclk_div > CK_DIV_1_6_PRESCALE_7_BYTE)
    {
        systemclk_div = CK_DIV_1_6_PRESCALE_7_BYTE;
    }
    /* If the system clock prescale is less than CK_DIV_1_6_PRESCALE_1_BYTE set it to
     * CK_DIV_1_6_PRESCALE_1_BYTE to avoid system clock being an unknown value */
    if (systemclk_div < CK_DIV_1_6_PRESCALE_1_BYTE)
    {
        systemclk_div = CK_DIV_1_6_PRESCALE_1_BYTE;
    }

    /* Start 48 MHz XTAL oscillator */
    Sys_Clocks_XTALClkConfig(systemclk_div);

    /* Switch to (divided 48 MHz) oscillator clock, and update the
     * SystemCoreClock global variable. */
    Sys_Clocks_SystemClkConfig(SYSCLK_CLKSRC_RFCLK);

    /* Configure clock dividers */
    Sys_Clocks_DividerConfig(UART_CLK, SENSOR_CLK, USER_CLK);

    /* Enable XTAL32k */
#if APP_PLATFORM == DEXCOM_HARDWARE
    ACS->XTAL32K_CTRL = XTAL32K_XIN_CAP_BYPASS_DISABLE | XTAL32K_NOT_FORCE_READY | XTAL32K_CTRIM_21P6PF |
                        XTAL32K_ITRIM_160NA | XTAL32K_ENABLE | XTAL32K_AMPL_CTRL_ENABLE;
#else    /* if APP_PLATFORM == DEXCOM_HARDWARE */
    ACS->XTAL32K_CTRL = XTAL32K_XIN_CAP_BYPASS_DISABLE | XTAL32K_NOT_FORCE_READY | XTAL32K_CTRIM_23P2PF |
                        XTAL32K_ITRIM_160NA | XTAL32K_ENABLE | XTAL32K_AMPL_CTRL_ENABLE;
#endif    /* APP_PLATFORM == DEXCOM_HARDWARE */

    /* Wait for XTAL32k to be configured */
    while (ACS->XTAL32K_CTRL && ((0x1U << ACS_XTAL32K_CTRL_READY_Pos) != XTAL32K_OK));
}

void App_Sleep_Initialization(void)
{
#if DEBUG_SLEEP_GPIO
    app_sleep_mode_cfg.app_gpio_config = App_GPIO_Config;
#else    /* if DEBUG_SLEEP_GPIO */
    app_sleep_mode_cfg.app_gpio_config = NULL;
#endif    /* if DEBUG_SLEEP_GPIO */

    /* DMA Channel number for RF register transfer */
    app_sleep_mode_cfg.DMA_channel_RF = 0;

    /* Wakeup Configuration */
    app_sleep_mode_cfg.wakeup_cfg = WAKEUP_DELAY_16                     |
                                    WAKEUP_GPIO1_ENABLE                 |
                                    WAKEUP_GPIO1_RISING                 |
                                    WAKEUP_DCDC_OVERLOAD_DISABLE;

    /* Clock Configuration for Run Mode */
    app_sleep_mode_cfg.clock_cfg.sensorclk_freq = SENSOR_CLK;
    app_sleep_mode_cfg.clock_cfg.systemclk_freq = SYSTEM_CLK;
    app_sleep_mode_cfg.clock_cfg.uartclk_freq = UART_CLK;
    app_sleep_mode_cfg.clock_cfg.userclk_freq = USER_CLK;

    /* VDD Retention Regulator Configuration */
    /*  By default TRIM values are set to 0x03. The user can to 0x00 to
     *  further reduce power consumption but this will limit operational
     *  capabilities across extended temperature range. */
    app_sleep_mode_cfg.vddret_ctrl.vddm_ret_trim = VDDMRETENTION_TRIM_MAXIMUM;
    app_sleep_mode_cfg.vddret_ctrl.vddc_ret_trim = VDDCRETENTION_TRIM_MAXIMUM;
    app_sleep_mode_cfg.vddret_ctrl.vddacs_ret_trim = VDDACSRETENTION_TRIM_MAXIMUM;

    /* Enable VDDT Baseband Timer if BLE is Present */
    app_sleep_mode_cfg.vddret_ctrl.vddt_ret = VDDTRETENTION_ENABLE;

    /* Ble functionality is present in this application */
    app_sleep_mode_cfg.ble_present = BLE_PRESENT;

    app_sleep_mode_cfg.boot_cfg = BOOT_FLASH_XTAL_DEFAULT_TRIM    |
                                  BOOT_PWR_CAL_BYPASS_ENABLE      |
                                  BOOT_ROT_BYPASS_ENABLE;
}

/**
 * @brief      Configures GPIOs to be used for test
 */
void App_GPIO_Config(void)
{
    /* Disable JTAG TDI, TDO, and TRST connections to GPIO 2, 3, and 4 */
    GPIO->JTAG_SW_PAD_CFG &= ~(CM33_JTAG_DATA_ENABLED | CM33_JTAG_TRST_ENABLED);

    /* Configure POWER_MODE_GPIO */
    SYS_GPIO_CONFIG(POWER_MODE_GPIO, (GPIO_2X_DRIVE | GPIO_LPF_DISABLE |
                                      GPIO_WEAK_PULL_UP | GPIO_MODE_GPIO_OUT));

    /* Clear POWER_MODE_GPIO to indicate Run Mode Configuration */
    Sys_GPIO_Set_Low(POWER_MODE_GPIO);

    /* Configure SYSCLK_GPIO to output system clock */
    SYS_GPIO_CONFIG(SYSCLK_GPIO, (GPIO_2X_DRIVE | GPIO_LPF_DISABLE |
                                  GPIO_WEAK_PULL_UP | GPIO_MODE_SYSCLK));

    /* Configure Wakeup activity GPIO for monitoring GPIO1 wakeup */
    SYS_GPIO_CONFIG(WAKEUP_ACTIVITY_GPIO, (GPIO_MODE_GPIO_OUT | GPIO_LPF_DISABLE |
                                           GPIO_WEAK_PULL_UP  | GPIO_6X_DRIVE));

    /* Configure Run activity GPIO for monitoring Task0 BLE ADV */
    SYS_GPIO_CONFIG(TASK0_RUN_ACTIVITY_GPIO, (GPIO_MODE_GPIO_OUT | GPIO_LPF_DISABLE |
                                              GPIO_WEAK_PULL_UP  | GPIO_6X_DRIVE));

    /* Configure Run activity GPIO for monitoring Task1 */
    SYS_GPIO_CONFIG(TASK1_RUN_ACTIVITY_GPIO, (GPIO_MODE_GPIO_OUT | GPIO_LPF_DISABLE |
                                              GPIO_WEAK_PULL_UP  | GPIO_6X_DRIVE));

    /* Configure Run activity GPIO for monitoring Task2 */
    SYS_GPIO_CONFIG(TASK2_RUN_ACTIVITY_GPIO, (GPIO_MODE_GPIO_OUT | GPIO_LPF_DISABLE |
                                              GPIO_WEAK_PULL_UP  | GPIO_6X_DRIVE));
}

/**
 * @brief Power Down the FPU Unit
 * @return FPU_Q_ACCEPTED if the FPU power down was successful.<br>
 *         FPU_Q_DENIED if the FPU power down failed.
 */
uint32_t Power_Down_FPU(void)
{
    uint32_t success;

    /* Request power down of FPU */
    SYSCTRL->FPU_PWR_CFG = FPU_WRITE_KEY | ((SYSCTRL->FPU_PWR_CFG &
                                             ~(0x1U << SYSCTRL_FPU_PWR_CFG_FPU_Q_REQ_Pos)) | FPU_Q_REQUEST);

    /* Wait until it is accepted or denied */
    while ((SYSCTRL->FPU_PWR_CFG & (0x1U << SYSCTRL_FPU_PWR_CFG_FPU_Q_ACCEPT_Pos)) !=
           FPU_Q_ACCEPTED &&
           (SYSCTRL->FPU_PWR_CFG & (0x1U << SYSCTRL_FPU_PWR_CFG_FPU_Q_DENY_Pos))   !=
           FPU_Q_DENIED);

    if ((SYSCTRL->FPU_PWR_CFG & (0x1U << SYSCTRL_FPU_PWR_CFG_FPU_Q_ACCEPT_Pos)) ==
        FPU_Q_ACCEPTED)
    {
        /* If it is accepted, isolate and power down FPU */
        SYSCTRL->FPU_PWR_CFG = FPU_WRITE_KEY | FPU_Q_REQUEST | FPU_ISOLATE |
                               FPU_PWR_TRICKLE_ENABLE  | FPU_PWR_HAMMER_ENABLE;
        SYSCTRL->FPU_PWR_CFG = FPU_WRITE_KEY | FPU_Q_REQUEST | FPU_ISOLATE |
                               FPU_PWR_TRICKLE_DISABLE | FPU_PWR_HAMMER_DISABLE;
        success = FPU_Q_ACCEPTED;
    }
    else
    {
        /* If it is denied, cancel request */
        SYSCTRL->FPU_PWR_CFG = FPU_WRITE_KEY | ((SYSCTRL->FPU_PWR_CFG &
                                                 ~(0x1U << SYSCTRL_FPU_PWR_CFG_FPU_Q_REQ_Pos)) | FPU_Q_NOT_REQUEST);
        success = FPU_Q_DENIED;
    }

    return success;
}

/**
 * @brief Power Down the DBG Unit
 * @return DBG_Q_ACCEPTED if the DBG power down was successful.<br>
 *         DBG_Q_DENIED if the DBG power down failed.
 */
uint32_t Power_Down_Debug(void)
{
    uint32_t success;

    /* Request power down of DBG */
    SYSCTRL->DBG_PWR_CFG = DBG_WRITE_KEY | ((SYSCTRL->DBG_PWR_CFG &
                                             ~(0x1U << SYSCTRL_DBG_PWR_CFG_DBG_Q_REQ_Pos)) | DBG_Q_REQUEST);

    /* Wait until it is accepted or denied */
    while ((SYSCTRL->DBG_PWR_CFG & (0x1U << SYSCTRL_DBG_PWR_CFG_DBG_Q_ACCEPT_Pos)) !=
           DBG_Q_ACCEPTED &&
           (SYSCTRL->DBG_PWR_CFG & (0x1U << SYSCTRL_DBG_PWR_CFG_DBG_Q_DENY_Pos))   !=
           DBG_Q_DENIED);

    if ((SYSCTRL->DBG_PWR_CFG & (0x1U << SYSCTRL_DBG_PWR_CFG_DBG_Q_ACCEPT_Pos)) ==
        DBG_Q_ACCEPTED)
    {
        /* If it is accepted, isolate and power down DBG */
        SYSCTRL->DBG_PWR_CFG = DBG_WRITE_KEY | DBG_Q_REQUEST | DBG_ISOLATE |
                               DBG_PWR_TRICKLE_ENABLE  | DBG_PWR_HAMMER_ENABLE;
        SYSCTRL->DBG_PWR_CFG = DBG_WRITE_KEY | DBG_Q_REQUEST | DBG_ISOLATE |
                               DBG_PWR_TRICKLE_DISABLE | DBG_PWR_HAMMER_DISABLE;
        success = DBG_Q_ACCEPTED;
    }
    else
    {
        /* If it is denied, cancel request */
        SYSCTRL->DBG_PWR_CFG = DBG_WRITE_KEY | ((SYSCTRL->DBG_PWR_CFG &
                                                 ~(0x1U << SYSCTRL_DBG_PWR_CFG_DBG_Q_REQ_Pos)) | DBG_Q_NOT_REQUEST);
        success = DBG_Q_DENIED;
    }

    return success;
}

/**
 * @brief Convert RTC cycles to time in number of
 *        Day/s Hour/s minutes/s second/s milisecond/s
 */
static void Convert_To_Time(uint64_t total_rtc_cycles)
{
    /* Divide by number of RTC Cycles in a day */
    current_time.day = (total_RTC_cycles / 2831155200);

    /* Divide by number of RTC Cycles in an hour and subtract day */
    current_time.hour = (total_RTC_cycles / 117964800) - (current_time.day * 24);

    /* Divide by number of RTC Cycles in an minute and subtract day & hour */
    current_time.min = (total_RTC_cycles / 1966080) - (current_time.day * 1440) - (current_time.hour * 60);

    /* Divide by number of RTC Cycles in an second and subtract day & hour & minute */
    current_time.sec = ((total_RTC_cycles / 32768) - (current_time.day * 86400) - (current_time.hour * 3600) \
                        - (current_time.min * 60));

    /* Divide by number of RTC Cycles in millisecond and subtract day & hour & minute & second */
    current_time.ms = ((total_RTC_cycles / 32.768) - (current_time.day * 86400000) - (current_time.hour * 3600000) \
                       - (current_time.min * 60000) - (current_time.sec * 1000));
}

/**
 * @brief Initialize swmTrace after wakeup from sleep
 */
void Init_SWMTrace(void)
{
    swmTrace_init(traceOptions, 5);
}

/**
 * @brief Print time information
 */
void Print_Time_Info(uint64_t total_rtc_cycles)
{
    /* Convert RTC cycles to time */
    Convert_To_Time(total_rtc_cycles);
    swmLogInfo("\n\rTotal RTC Cycles: %d\n\r", total_rtc_cycles);
    swmLogInfo("\n\rDays: %d\n\rHours: %d\n\rMins: %d\n\rSecs: %d\n\rMillisecs: %d\n\r",
               current_time.day, current_time.hour, current_time.min, current_time.sec, current_time.ms);

    /* Delay for logs */
    Sys_Delay(0.025 * SystemCoreClock);
}
