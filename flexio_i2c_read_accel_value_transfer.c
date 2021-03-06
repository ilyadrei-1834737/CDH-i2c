/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*  Standard C Included Files */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*  SDK Included Files */
#include "board.h"
#include "fsl_debug_console.h"
#include "fsl_flexio_i2c_master.h"

#include "pin_mux.h"
#include "clock_config.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define BOARD_FLEXIO_BASE FLEXIO2

/* Select USB1 PLL (480 MHz) as flexio clock source */
#define FLEXIO_CLOCK_SELECT (3U)
/* Clock pre divider for flexio clock source */
#define FLEXIO_CLOCK_PRE_DIVIDER (1U)
/* Clock divider for flexio clock source */
#define FLEXIO_CLOCK_DIVIDER (5U)
#define FLEXIO_CLOCK_FREQUENCY \
    (CLOCK_GetFreq(kCLOCK_Usb1PllClk) / (FLEXIO_CLOCK_PRE_DIVIDER + 1U) / (FLEXIO_CLOCK_DIVIDER + 1U))

#define FLEXIO_I2C_SDA_PIN 5U
#define FLEXIO_I2C_SCL_PIN 6U

#define I2C_BAUDRATE (100000) /* 100K */

#define FXOS8700_WHOAMI (0xC7U)
#define MMA8451_WHOAMI (0x1AU)
#define ACCEL_STATUS (0x00U)
#define ACCEL_XYZ_DATA_CFG (0x0EU)
#define ACCEL_CTRL_REG1 (0x2AU)
/* FXOS8700 and MMA8451 have the same who_am_i register address. */
#define ACCEL_WHOAMI_REG (0x0DU)
#define ACCEL_READ_TIMES (10U)

#define HMC5983_ADDR7BIT (0x1EU)
#define HMC5983_WHOAMI (0x0AU)
#define HMC5983_STATUS (0x09U)
#define HMC5983_CRA (0x00U)
#define HMC5983_CRB (0x01U)
#define HMC5983_MODE (0x02U)
#define HMC5983_XMSB (0x03U)
#define HMC5983_XLSB (0x04U)
#define HMC5983_ZMSB (0x05U)
#define HMC5983_ZLSB (0x06U)
#define HMC5983_YMSB (0x07U)
#define HMC5983_YLSB (0x08U)

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

static bool I2C_example_readAccelWhoAmI(void);
static bool I2C_write_accel_reg(FLEXIO_I2C_Type *base, uint8_t device_addr, uint8_t reg_addr, uint8_t value);
static bool I2C_read_accel_regs(
    FLEXIO_I2C_Type *base, uint8_t device_addr, uint8_t reg_addr, uint8_t *rxBuff, uint32_t rxSize);

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*  FXOS8700 and MMA8451 device address */
const uint8_t g_accel_address[] = {0x1CU, 0x1DU, 0x1EU, 0x1FU};

flexio_i2c_master_handle_t g_m_handle;
FLEXIO_I2C_Type i2cDev;
uint8_t g_accel_addr_found   = 0x00;
volatile bool completionFlag = false;
volatile bool nakFlag        = false;

/*******************************************************************************
 * Code
 ******************************************************************************/

static void flexio_i2c_master_callback(FLEXIO_I2C_Type *base,
                                       flexio_i2c_master_handle_t *handle,
                                       status_t status,
                                       void *userData)
{
    /* Signal transfer success when received success status. */
    if (status == kStatus_Success)
    {
        completionFlag = true;
    }
    /* Signal transfer success when received success status. */
    if (status == kStatus_FLEXIO_I2C_Nak)
    {
        nakFlag = true;
    }
}

static bool I2C_example_readAccelWhoAmI(void)
{
    /*
    How to read the device who_am_I value ?
    Start + Device_address_Write , who_am_I_register;
    Repeart_Start + Device_address_Read , who_am_I_value.
    */
    uint8_t who_am_i_reg          = ACCEL_WHOAMI_REG;
    uint8_t who_am_i_value        = 0x00;
    uint8_t accel_addr_array_size = 0x00;
    bool find_device              = false;
    bool result                   = false;
    uint8_t i                     = 0;
    uint32_t j                    = 0;

    flexio_i2c_master_config_t masterConfig;


    masterConfig.enableMaster = true;
    masterConfig.enableInDoze = false;
    masterConfig.enableInDebug = true;
    masterConfig.enableFastAccess = false;
    masterConfig.baudRate_Bps = 100000U;

    FLEXIO_I2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Bps = I2C_BAUDRATE;

    /* initialize clock */
    if (FLEXIO_I2C_MasterInit(&i2cDev, &masterConfig, FLEXIO_CLOCK_FREQUENCY) != kStatus_Success)
    {
        PRINTF("FlexIO clock frequency exceeded upper range. \r\n");
        return false;
    }

    /* settings for transfer */
    flexio_i2c_master_transfer_t masterXfer;
    memset(&masterXfer, 0, sizeof(masterXfer));

    /* g_accel_address[0] -> HMC5983_ADDR7BIT */
    masterXfer.slaveAddress   = HMC5983_ADDR7BIT;
    masterXfer.direction      = kFLEXIO_I2C_Read;
    masterXfer.subaddress     = who_am_i_reg;
    masterXfer.subaddressSize = 1;
    masterXfer.data           = &who_am_i_value;
    masterXfer.dataSize       = 1;

    accel_addr_array_size = sizeof(g_accel_address) / sizeof(g_accel_address[0]);

    for (i = 0; i < accel_addr_array_size; i++)
    {
        masterXfer.slaveAddress = g_accel_address[i];
        completionFlag          = false;
        FLEXIO_I2C_MasterTransferNonBlocking(&i2cDev, &g_m_handle, &masterXfer);

        /*  wait for transfer completed. */
        while ((nakFlag == false) && (completionFlag == false))
        {
        }
        if (nakFlag == true)
        {
            nakFlag = false;
            for (j = 0; j < 0x1FFF; j++)
            {
                __NOP();
            }
            continue;
        }
        if (completionFlag == true)
        {
            completionFlag     = false;
            find_device        = true;
            g_accel_addr_found = masterXfer.slaveAddress;
            break;
        }
        for (j = 0; j < 0xFFF; j++)
        {
            __NOP();
        }
    }

    if (find_device == true)
    {
        switch (who_am_i_value)
        {
            case FXOS8700_WHOAMI:
                PRINTF("Found a FXOS8700 on board, the device address is 0x%02X. \r\n", masterXfer.slaveAddress);
                result = true;
                break;
            case MMA8451_WHOAMI:
                PRINTF("Found a MMA8451 on board, the device address is 0x%02X. \r\n", masterXfer.slaveAddress);
                result = true;
                break;
            default:

                PRINTF("Found a device, the WhoAmI value is 0x%02X\r\n", who_am_i_value);
                PRINTF("It's not MMA8451 or FXOS8700. \r\n");
                PRINTF("The device address is 0x%02X. \r\n", masterXfer.slaveAddress);
                result = false;
                break;
        }

        return result;
    }
    else
    {
        PRINTF("Not a successful i2c communication\r\n");
        return false;
    }
}

static bool I2C_write_accel_reg(FLEXIO_I2C_Type *base, uint8_t device_addr, uint8_t reg_addr, uint8_t value)
{
    flexio_i2c_master_transfer_t masterXfer;
    memset(&masterXfer, 0, sizeof(masterXfer));

    masterXfer.slaveAddress   = device_addr;
    masterXfer.direction      = kFLEXIO_I2C_Write;
    masterXfer.subaddress     = reg_addr;
    masterXfer.subaddressSize = 1;
    masterXfer.data           = &value;
    masterXfer.dataSize       = 1;

    /*  direction=write : start+device_write;cmdbuff;xBuff; */
    /*  direction=recive : start+device_write;cmdbuff;repeatStart+device_read;xBuff; */

    FLEXIO_I2C_MasterTransferNonBlocking(&i2cDev, &g_m_handle, &masterXfer);

    /*  Wait for transfer completed. */
    while ((!nakFlag) && (!completionFlag))
    {
    }

    nakFlag = false;

    if (completionFlag == true)
    {
        completionFlag = false;
        return true;
    }
    else
    {
        return false;
    }
}

static bool I2C_read_accel_regs(
    FLEXIO_I2C_Type *base, uint8_t device_addr, uint8_t reg_addr, uint8_t *rxBuff, uint32_t rxSize)
{
    flexio_i2c_master_transfer_t masterXfer;
    memset(&masterXfer, 0, sizeof(masterXfer));
    masterXfer.slaveAddress   = device_addr;
    masterXfer.direction      = kFLEXIO_I2C_Read;
    masterXfer.subaddress     = reg_addr;
    masterXfer.subaddressSize = 1;
    masterXfer.data           = rxBuff;
    masterXfer.dataSize       = rxSize;

    /*  direction=write : start+device_write;cmdbuff;xBuff; */
    /*  direction=recive : start+device_write;cmdbuff;repeatStart+device_read;xBuff; */

    FLEXIO_I2C_MasterTransferNonBlocking(&i2cDev, &g_m_handle, &masterXfer);

    /*  Wait for transfer completed. */
    while ((!nakFlag) && (!completionFlag))
    {
    }

    nakFlag = false;

    if (completionFlag == true)
    {
        completionFlag = false;
        return true;
    }
    else
    {
        return false;
    }
}

/*!
 * @brief Main function
 */
int main(void)
{
    bool isThereAccel = false;

    /*do hardware configuration*/
    i2cDev.flexioBase      = BOARD_FLEXIO_BASE;
    i2cDev.SDAPinIndex     = FLEXIO_I2C_SDA_PIN;
    i2cDev.SCLPinIndex     = FLEXIO_I2C_SCL_PIN;
    i2cDev.shifterIndex[0] = 0U;
    i2cDev.shifterIndex[1] = 1U;
    i2cDev.timerIndex[0]   = 0U;
    i2cDev.timerIndex[1]   = 1U;

    BOARD_ConfigMPU();
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    /* Clock setting for Flexio */
    CLOCK_SetMux(kCLOCK_Flexio2Mux, FLEXIO_CLOCK_SELECT);
    CLOCK_SetDiv(kCLOCK_Flexio2PreDiv, FLEXIO_CLOCK_PRE_DIVIDER);
    CLOCK_SetDiv(kCLOCK_Flexio2Div, FLEXIO_CLOCK_DIVIDER);

    PRINTF("\r\nI2C BOYS\r\n");

    FLEXIO_I2C_MasterTransferCreateHandle(&i2cDev, &g_m_handle, flexio_i2c_master_callback, NULL);
    // isThereAccel = I2C_example_readAccelWhoAmI();

    PRINTF("\r\n HANDLE CREATED");

	uint8_t databyte  = 0;
	uint8_t write_reg = 0; //0
	uint8_t readBuff[7];
	int16_t x, y, z;
	uint8_t status0_value = 0;
	uint32_t i            = 0;

	PRINTF("\r\n SET CONFIG CRA \r\n");
	/* SET CONFIG REG A TO 0x70 */
	write_reg = HMC5983_CRA;
	databyte = 0x70U;
	g_accel_addr_found = HMC5983_ADDR7BIT;
	I2C_write_accel_reg(&i2cDev, g_accel_addr_found, write_reg, databyte);

	PRINTF("\r\n SET CONFIG CRB \r\n");
	/* SET CONFIG REG B to 0xA0 */
	write_reg = HMC5983_CRB;
	databyte = 0xA0U;
	g_accel_addr_found = HMC5983_ADDR7BIT;
	I2C_write_accel_reg(&i2cDev, g_accel_addr_found, write_reg, databyte);

	PRINTF("\r\n CONTINUOUS REGISTER MODE \r\n");
	/* WRITE 0 TO MODE REGISTER FOR CONTINUOUS MEASUREMENT */
	write_reg = HMC5983_MODE;
	databyte  = 0;
	g_accel_addr_found = HMC5983_ADDR7BIT;
	I2C_write_accel_reg(&i2cDev, g_accel_addr_found, write_reg, databyte);


	PRINTF("The accel values:\r\n");
	for (i = 0; i < ACCEL_READ_TIMES; i++)
	{

		/* CHECK STATUS */ PRINTF("\r\nCHECK STATUS\r\n");
		status0_value = 0;
		while (status0_value != 0xff)
		{
			g_accel_addr_found = HMC5983_ADDR7BIT;
			I2C_read_accel_regs(&i2cDev, g_accel_addr_found, HMC5983_STATUS, &status0_value, 1);
		}


		/*multi byte read starting from first data register (0x03) */

		g_accel_addr_found = HMC5983_ADDR7BIT;
		I2C_read_accel_regs(&i2cDev, g_accel_addr_found, HMC5983_XMSB, readBuff, 7);

		status0_value = readBuff[0];
		x             = ((int16_t)(((readBuff[1] * 256U) | readBuff[2]))) / 4U;
		y             = ((int16_t)(((readBuff[3] * 256U) | readBuff[4]))) / 4U;
		z             = ((int16_t)(((readBuff[5] * 256U) | readBuff[6]))) / 4U;

		PRINTF("status_reg = 0x%x , x = %5d , y = %5d , z = %5d \r\n", status0_value, x, y, z);
	}

    PRINTF("\r\nEnd of I2C example .\r\n");
    while (1)
    {
    }
}
