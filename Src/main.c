/*****************************************************************************
	*	MULTICALOR NV
	*	
	*	File:	main.c
	*		Update regularly the flags
	*
	******************************************************************************
	*	Verion V1.0.0
	*	Date: 21-01-2015
	*	Main program body
	******************************************************************************
*/

/*****************************************
	*	Newlib libc
	*
	******************************************
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*****************************************
	*	Include peripheral drivers
	*
	******************************************
*/
#include "main.h"
#include "stm32f4xx_it.h"

/*****************************************
	*	Include exported libs
	*
	******************************************
*/
#include "../drivers/display/ili_9341.h"
#include "stm32_ub_stemwin.h"
#include "GUI_verventa.h"
/*	Modbus includes --------------------------------------------------*/
#include "mb.h"
#include "mb_m.h"
#include "mbport.h"


/* Private Functions -----------------------------------------------------*/
static void SystemClock_Config(void);
static void Error_Handler(void);
static void init_leds(void);
static void sram_init(void);
static void I2C_init(void);
static void USART1_init(void);
static void timer_4_init(void);
static void timer_5_init(void);
static void Timer9_Init(void);

/* Private macro -----------------------------------------------------*/
/* Private variables -------------------------------------------------*/
SRAM_HandleTypeDef			hsram;
FMC_NORSRAM_TimingTypeDef	SRAM_Timing;
I2C_HandleTypeDef			I2cHandle;
UART_HandleTypeDef			UartHandle;
SD_HandleTypeDef			hsd;
HAL_SD_CardInfoTypedef		SDCardInfo;
TIM_HandleTypeDef			TimHandle_9;
TIM_HandleTypeDef 			TimHandle_4;
TIM_HandleTypeDef 			TimHandle_5;

bool            xtNeedPoll		= true;
static uint16_t usMRegInStart	= M_REG_INPUT_START;						//Input Register variables
static uint16_t usMRegInBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_INPUT_NREGS];	//Input Register variables
static uint16_t	xtinputNRegs;
static uint16_t	xtinputRegIndex;

static uint16_t	usMRegHoldStart	= M_REG_HOLDING_START;							//HoldingRegister variables
static uint16_t	usMRegHoldBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_HOLDING_NREGS];	//HoldingRegister variables
static uint16_t	xtholdingNRegs;
static uint16_t	xtholdingRegIndex;

static uint16_t usMCoilStart	= M_COIL_START;
#if      M_COIL_NCOILS%8
uint8_t    ucMCoilBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_COIL_NCOILS/8+1];
#else
uint8_t    ucMCoilBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_COIL_NCOILS/8];
#endif
static uint16_t	xtCoilNRegs;
static uint16_t	xtCoilRegIndex;
/*	FATfs	*/
FATFS	SDFatFs;  /* File system object for SD card logical drive */
char	SDPath[4]; /* SD card logical drive path */

/* External types ------------------------------------------------------------*/
extern bool gui_Exec;
extern bool modbus_Exec;
extern bool Touch_Flagged;

/* Defines -----------------------------------------------------------*/
#define	I2C_ADDRESS		0xFE

//****************************************************************************
/* Static variables --------------------------------------------------*/
static int Debug_ITMDebug = 0;

//****************************************************************************

void Debug_ITMDebugEnable(void){
	volatile unsigned int *ITM_TER      = (volatile unsigned int *)0xE0000E00;
	volatile unsigned int *SCB_DHCSR 		= (volatile unsigned int *)0xE000EDF0;
	volatile unsigned int *DBGMCU_CR 		= (volatile unsigned int *)0xE0042004;
	
	*DBGMCU_CR |= 0x27; // DBGMCU_CR
	
	if ((*SCB_DHCSR & 1) && (*ITM_TER & 1)) // Enabled?
    Debug_ITMDebug = 1;
}

//****************************************************************************

void Debug_ITMDebugOutputChar(char ch){
	static volatile unsigned int *ITM_STIM0 = (volatile unsigned int *)0xE0000000; // ITM Port 0
	static volatile unsigned int *SCB_DEMCR = (volatile unsigned int *)0xE000EDFC;
	
	if (Debug_ITMDebug && (*SCB_DEMCR & 0x01000000))
	{
		while(*ITM_STIM0 == 0);
		*((volatile char *)ITM_STIM0) = ch;
	}
}

//****************************************************************************

void Debug_ITMDebugOutputString(char *Buffer){
	if (Debug_ITMDebug)
	while(*Buffer)
	Debug_ITMDebugOutputChar(*Buffer++);
}

//******************************************************************************


/**
	* @brief	main().
	* @param	None
	*	@note	Init the system.
	*			Setup modbus.
	*			Setup emWin.
	* 		Update GUI every 10ms.
	* 		Update Touch every 1ms.
	* 		return here after a call.
	*	@retval	None
*/
int main(void){
	uint16_t			i;
	uint16_t 			counter = 1;
	eMBErrorCode		eStatus;
	DSTATUS				Disk_State;
	uint8_t				Disk_label[12];
	eMBMasterReqErrCode	errorCode	= MB_MRE_NO_ERR;
	/* STM32f4xx HAL library initialization:
		- Cofigure Flash prefetch, flash preread and Buffer caches
		- Systick timer config
		- Low level init
	*/
	HAL_Init();
	/*********************************************************************/
	
	/* configure the system clock to 168 MHz */
	SystemClock_Config();
	
	/* Enable TRACE debug -----------------------------------------------*/
	Debug_ITMDebugEnable();
 	Debug_ITMDebugOutputString("SWV Enabled\n");
	/*********************************************************************/
	
	/* Init peripherals -------------------------------------------------*/
	sram_init();
	I2C_init();
	USART1_init();
	Timer9_Init();
	timer_4_init();
	timer_5_init();
	/*********************************************************************/
	
	/* Show us some status leds ----------------------------------------*/
	init_leds();
	
	/*********************************************************************/
	/*	Init modbus	Master ----------------------------------------------*/
	if((eMBMasterInit(MB_RTU, 0, 115200, MB_PAR_NONE)) != MB_ENOERR){
		printf("eStatus Master Init: %s\n", eStatus ? "error": "no'error");
		Error_Handler();
	}
	
	/*	Enable the Modbus Protocol Stack --------------------------------*/
	if((eMBMasterEnable()) != MB_ENOERR){
		printf("eStatus Master Enable: %s\n", eStatus ? "error": "no'error");
		Error_Handler();
	}
	
	/*	Init STemWin V5.2.6 and GUI -------------------------------------*/
	if((UB_STemWIN_init()) != SUCCESS){
		printf("emWin INIT error...");
	}
	
	/*	Init Verventa GUI -----------------------------------------------*/
	if(( Init_GUI_verventa()) != SUCCESS){
		printf("can't INIT verventa GUI...");
	}
	
	/*	Link disk drive to filesystem */
	if(FATFS_LinkDriver(&SD_Driver, SDPath) == 0){
		printf("SD driver linked...\n");
		if(f_mount(&SDFatFs, (TCHAR const*)SDPath, 1) != FR_OK)
		{
			/* FatFs Initialization Error */
			Error_Handler();
		}
		else
		{
			/*if(f_mkfs((TCHAR const*)SDPath, 0, 0) != FR_OK)
			{
				/* FatFs Format Error /
				Error_Handler();
			}*/
			if(f_setlabel("LOG DISK") != FR_OK)
			{
				Error_Handler();
			}
			else{
				if(f_getlabel((TCHAR const*)SDPath, Disk_label, 0) != FR_OK)
				{
					Error_Handler();
				}
				else{
					printf("Disk Label: %s\n", Disk_label);
				}
			}
		}
		Disk_State = disk_status(0);
		printf("Disk_State: %d\n", Disk_State);
	}
	
	
	while(1){
		
		//	printf("Compiled HAL\n");
		
		//	test_Cplusplus();
		
		if(xtNeedPoll == true){
			printf("xtNeedPoll: %s\n", xtNeedPoll ? "true" : "false");
			counter++;
			
			for(i = 0; i <= xtholdingNRegs; i++){
					printf("usMRegHoldBuf[%d]: %d\n", xtholdingRegIndex + i, usMRegHoldBuf[ucMBMasterGetDestAddress() - 1][xtholdingRegIndex + i]);
			}
			/*for(i = 0; i <= xtinputNRegs; i++){
					printf("usMRegInBuf[%d]: %d\n", xtinputRegIndex + i, usMRegInBuf[ucMBMasterGetDestAddress() - 1][xtinputRegIndex + i]);
			}*/
			for(i = 0; i <= xtCoilNRegs; i++){
					printf("ucMCoilBuf[%d]: %d\n", xtCoilRegIndex + i, ucMCoilBuf[ucMBMasterGetDestAddress() - 1][xtCoilRegIndex + i]);
			}

			switch(counter % 2){
			case 0:
				//errorCode = eMBMasterReqReadInputRegister(10, 497, 6, -1);
				errorCode = eMBMasterReqReadHoldingRegister(10, 147, 20, -1);
				
				break;
			case 1:
				//errorCode = eMBMasterReqReadHoldingRegister(10, 147, 6, -1);
				//errorCode = eMBMasterReqReadInputRegister(10, 497, 15, -1);
				errorCode = eMBMasterReqReadCoils(10,79,25,-1);
				
				break;
			default:
				break;
			}
			
			xtNeedPoll = false;
		}
		
		if(modbus_Exec == true){
			eStatus = eMBMasterPoll();
			
			/*	Clear Flag */
			modbus_Exec = false;
		}
		
		if(gui_Exec == true){
			WM_InvalidateWindow(hWinFanImg);
			GUI_Exec();
			//eStatus = eMBMasterPoll();
			//printf("SystemCoreClock: %d\n", SystemCoreClock);
			
			/*	Clear Flag */
			gui_Exec = false;
		}
		
		if(Touch_Flagged == true){
			//UB_STemWIN_updateTouch();
			
			/*	Clear Flag */
			Touch_Flagged = false;
		}
		
	}
}

/**
  * @brief	Builds the Input Registers frame.
  * @param	*pucRegBuffer
  * @param	usAddress
  * @param	usNRegs
  * @retval	eMBErrorCode
  */
eMBErrorCode
eMBMasterRegInputCB(uint8_t* pucRegBuffer, uint16_t usAddress, uint16_t usNRegs)
{
	eMBErrorCode    eStatus = MB_ENOERR;
    uint16_t          iRegIndex;
    uint16_t *        pusRegInputBuf;
    uint16_t          REG_INPUT_START;
    uint16_t          REG_INPUT_NREGS;
    uint16_t          usRegInStart;
	uint8_t			MasterDestAddress;
	
	MasterDestAddress = ucMBMasterGetDestAddress();
	printf("MasterDestAddress: %d\n", MasterDestAddress);
	
	xtinputNRegs = usNRegs;

    pusRegInputBuf = usMRegInBuf[ucMBMasterGetDestAddress() - 1];
    REG_INPUT_START = M_REG_INPUT_START;
    REG_INPUT_NREGS = M_REG_INPUT_NREGS;
    usRegInStart = usMRegInStart;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= REG_INPUT_START)
            && (usAddress + usNRegs <= REG_INPUT_START + REG_INPUT_NREGS))
    {
        iRegIndex = usAddress - usRegInStart;
		xtinputRegIndex = iRegIndex;
        while (usNRegs > 0)
        {
            pusRegInputBuf[iRegIndex] = *pucRegBuffer++ << 8;
            pusRegInputBuf[iRegIndex] |= *pucRegBuffer++;
            iRegIndex++;
            usNRegs--;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }

    return eStatus;
}

/**
  * @brief	Builds the Holding Registers frame.
  * @param	*pucRegBuffer
  * @param	usAddress
  * @param	usNRegs
  * @param	eMode
  * @retval	eMBErrorCode
  */
eMBErrorCode
eMBMasterRegHoldingCB(uint8_t* pucRegBuffer, uint16_t usAddress, uint16_t usNRegs, eMBRegisterMode eMode)
{
    eMBErrorCode    eStatus = MB_ENOERR;
    uint16_t          iRegIndex;
    uint16_t *        pusRegHoldingBuf;
    uint16_t          REG_HOLDING_START;
    uint16_t          REG_HOLDING_NREGS;
    uint16_t          usRegHoldStart;
	
	xtholdingNRegs = usNRegs;

    pusRegHoldingBuf = usMRegHoldBuf[ucMBMasterGetDestAddress() - 1];
    REG_HOLDING_START = M_REG_HOLDING_START;
    REG_HOLDING_NREGS = M_REG_HOLDING_NREGS;
    usRegHoldStart = usMRegHoldStart;
    /* if mode is read, the master will write the received date to buffer. */
    eMode = MB_REG_WRITE;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= REG_HOLDING_START)
            && (usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS))
    {
        iRegIndex = usAddress - usRegHoldStart;
		xtholdingRegIndex = iRegIndex;
        switch (eMode)
        {
        /* read current register values from the protocol stack. */
        case MB_REG_READ:
            while (usNRegs > 0)
            {
                *pucRegBuffer++ = (uint8_t) (pusRegHoldingBuf[iRegIndex] >> 8);
                *pucRegBuffer++ = (uint8_t) (pusRegHoldingBuf[iRegIndex] & 0xFF);
                iRegIndex++;
                usNRegs--;
            }
            break;
        /* write current register values with new values from the protocol stack. */
        case MB_REG_WRITE:
            while (usNRegs > 0)
            {
                pusRegHoldingBuf[iRegIndex] = *pucRegBuffer++ << 8;
                pusRegHoldingBuf[iRegIndex] |= *pucRegBuffer++;
                iRegIndex++;
                usNRegs--;
            }
            break;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
}

/**
  * @brief	Builds the Coils frame.
  * @param	*pucRegBuffer
  * @param	usAddress
  * @param	usNCoils
  * @param	eMode
  * @retval	eMBErrorCode
  */
eMBErrorCode
eMBMasterRegCoilsCB(uint8_t* pucRegBuffer, uint16_t usAddress, uint16_t usNCoils, eMBRegisterMode eMode)
{
    eMBErrorCode    eStatus = MB_ENOERR;
    uint16_t          iRegIndex , iRegBitIndex , iNReg;
    uint8_t *         pucCoilBuf;
    uint16_t          COIL_START;
    uint16_t          COIL_NCOILS;
    uint16_t          usCoilStart;
	
    iNReg =  usNCoils / 8 + 1;
	xtCoilNRegs = iNReg;

    pucCoilBuf = ucMCoilBuf[ucMBMasterGetDestAddress() - 1];
    COIL_START = M_COIL_START;
    COIL_NCOILS = M_COIL_NCOILS;
    usCoilStart = usMCoilStart;

    /* if mode is read,the master will write the received date to buffer. */
    eMode = MB_REG_WRITE;

    /* it already plus one in modbus function method. */
    usAddress--;

    if ((usAddress >= COIL_START)
            && (usAddress + usNCoils <= COIL_START + COIL_NCOILS))
    {
        iRegIndex = (uint16_t) (usAddress - usCoilStart) / 8;
		xtCoilRegIndex = iRegIndex;
        iRegBitIndex = (uint16_t) (usAddress - usCoilStart) % 8;
        switch (eMode)
        {
         /* read current coil values from the protocol stack. */
        case MB_REG_READ:
            while (iNReg > 0)
            {
                *pucRegBuffer++ = xMBUtilGetBits(&pucCoilBuf[iRegIndex++],
                        iRegBitIndex, 8);
                iNReg--;
            }
            pucRegBuffer--;
            /* last coils */
            usNCoils = usNCoils % 8;
            /* filling zero to high bit */
            *pucRegBuffer = *pucRegBuffer << (8 - usNCoils);
            *pucRegBuffer = *pucRegBuffer >> (8 - usNCoils);
            break;

        /* write current coil values with new values from the protocol stack. */
        case MB_REG_WRITE:
            while (iNReg > 1)
            {
                xMBUtilSetBits(&pucCoilBuf[iRegIndex++], iRegBitIndex, 8,
                        *pucRegBuffer++);
                iNReg--;
            }
            /* last coils */
            usNCoils = usNCoils % 8;
            /* xMBUtilSetBits has bug when ucNBits is zero */
            if (usNCoils != 0)
            {
                xMBUtilSetBits(&pucCoilBuf[iRegIndex++], iRegBitIndex, usNCoils,
                        *pucRegBuffer++);
            }
            break;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }
    return eStatus;
}

/**
  * @brief	Builds the Discrete frame.
  * @param	*pucRegBuffer
  * @param	usAddress
  * @param	usNDiscrete
  * @retval	eMBErrorCode
  */
eMBErrorCode
eMBMasterRegDiscreteCB(uint8_t* pucRegBuffer, uint16_t usAddress, uint16_t usNDiscrete)
{
    ( void )pucRegBuffer;
    ( void )usAddress;
    ( void )usNDiscrete;
    return MB_ENOREG;
}

/**
	* @brief	System Clock configuration
	*		System clock Source	= PLL(HSE)
	*		SYSCLK(Hz)		= 168000000
	*		HCLK(Hz)		= 168000000
	*		AHB Prescale		= 1
	*		APB1 Prescale		= 4
	*		APB2 Prescale		= 2
	*		HSE frequency(Hz)	= 25000000
	*		PLL_M			= 25
	*		PLL_N			= 336
	*		PLL_P			= 2
	*		PLL_Q			= 7
	*		VDD(V)			= 3.3
	*		Main reg out Volt	= Scale1 mode
	*		Flash lat(ws)		= 5
	*	@param None
	*	@retval None
*/
static void SystemClock_Config(void){
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_OscInitTypeDef RCC_OscInitStruct;
	
	/* Enable Power control clock */
	__HAL_RCC_PWR_CLK_ENABLE();
	
	/* The voltage scaling allows optimizing the power consumption when the
		device is clocked below the maximum system frequency, to update the
		voltage scaling value regarding system frequency refer to product
	datasheet. */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
	
	/* Enable HSE Oscillator and activate PLL with HSE as source */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 25;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK){
		/* Initialization Error */
		Error_Handler();
	}
	/* Select PLL as system clock source and configure the HCLK, PCLK1
	and PCLK2 clocks dividers */
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK	| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
	if(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK){
		/* Initialization Error */
		Error_Handler();
	}
	
	/* STM32F407x Revision Z devices: prefetch is supported */
	if(HAL_GetREVID() == 0x1001){
		/* Enable the Flash prefetch */
		__HAL_FLASH_PREFETCH_BUFFER_ENABLE();
	}
}


/**
	* @brief	Handles general errors.
	* @param	None
	* @retval	None
*/
static void Error_Handler(void){
	
	while(1){
		/* Put error on LED3 */
		HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
		printf("Peripheral config error\n");
	}
}

/**
	* @brief	Init leds PD12-13
	* @param	None
	* @retval	None
*/
static void init_leds(void){
	GPIO_InitTypeDef GPIO_InitStructure;
	
	// Enable AHB1per Clock
	__GPIOD_CLK_ENABLE();
	
	//Config pins
	GPIO_InitStructure.Pin		= GPIO_PIN_13;
	GPIO_InitStructure.Mode		= GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Speed	= GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull		= GPIO_PULLUP;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStructure);
}

/**
	* @brief init SRAM
	* @param None
	* @retval None
*/
static void sram_init(void){
	/*	Configure the FMC bus ---------------------------*/
	hsram.Instance  = FMC_NORSRAM_DEVICE;
	
	SRAM_Timing.AddressSetupTime       = 2;
	SRAM_Timing.AddressHoldTime        = 1;
	SRAM_Timing.DataSetupTime          = 2;
	SRAM_Timing.BusTurnAroundDuration  = 1;
	SRAM_Timing.CLKDivision            = 2;
	SRAM_Timing.DataLatency            = 2;
	SRAM_Timing.AccessMode             = FMC_ACCESS_MODE_A;
	
	hsram.Init.NSBank				= FMC_NORSRAM_BANK1;
	hsram.Init.DataAddressMux		= FMC_DATA_ADDRESS_MUX_DISABLE;
	hsram.Init.MemoryType			= FMC_MEMORY_TYPE_SRAM;
	hsram.Init.MemoryDataWidth		= FMC_NORSRAM_MEM_BUS_WIDTH_16;
	hsram.Init.BurstAccessMode		= FMC_BURST_ACCESS_MODE_DISABLE;
	hsram.Init.WaitSignalPolarity	= FMC_WAIT_SIGNAL_POLARITY_LOW;
	hsram.Init.WrapMode				= FMC_WRAP_MODE_DISABLE;
	hsram.Init.WaitSignalActive		= FMC_WAIT_TIMING_BEFORE_WS;
	hsram.Init.WriteOperation		= FMC_WRITE_OPERATION_ENABLE;
	hsram.Init.WaitSignal			= FMC_WAIT_SIGNAL_DISABLE;
	hsram.Init.ExtendedMode			= FMC_EXTENDED_MODE_DISABLE;
	hsram.Init.AsynchronousWait		= FMC_ASYNCHRONOUS_WAIT_DISABLE;
	hsram.Init.WriteBurst			= FMC_WRITE_BURST_DISABLE;
	hsram.Init.ContinuousClock    	= FMC_CONTINUOUS_CLOCK_SYNC_ASYNC;
	
	/* Initialize the SRAM controller ----------------------------*/
	if(HAL_SRAM_Init(&hsram, &SRAM_Timing, &SRAM_Timing) != HAL_OK){
		/* Init Error */
		Error_Handler();
	}
}

/**
	* @brief init I2C1
	* @param None
	* @retval None
*/
static void I2C_init(void){
	/*	Configure the I2C peripheral ---------------------------*/
	I2cHandle.Instance             = I2Cx;
	
	I2cHandle.Init.AddressingMode	= I2C_ADDRESSINGMODE_7BIT;
	I2cHandle.Init.ClockSpeed		= 400000;
	I2cHandle.Init.DualAddressMode	= I2C_DUALADDRESS_DISABLE;
	I2cHandle.Init.DutyCycle		= I2C_DUTYCYCLE_2;
	I2cHandle.Init.GeneralCallMode	= I2C_GENERALCALL_ENABLE;
	I2cHandle.Init.NoStretchMode	= I2C_NOSTRETCH_DISABLE;
	I2cHandle.Init.OwnAddress1		= I2C_ADDRESS;
	I2cHandle.Init.OwnAddress2		= I2C_ADDRESS;
	
	if(HAL_I2C_Init(&I2cHandle) != HAL_OK)
	{
		/* Initialization Error */
		Error_Handler();    
	}
	
}

/**
	* @brief init USART1
	* @param None
	* @retval None
*/
static void USART1_init(void){
	uint32_t baudrate = 115200;
	/*	Configure the USART1 peripheral in the Asynchronous mode (UART Mode)------*/
	/*	Configure the USART1 peripheral in the Asynchronous mode (UART Mode)------*/
	/*UART1 configured as follow:
		- Word Length	= 8 Bits
		- Stop Bit		= One Stop bit
		- Parity		= None
		- BaudRate		= 115200 baud
		- Hardware flow control disabled (RTS and CTS signals) */
	UartHandle.Instance          = USARTx;
	
	UartHandle.Init.BaudRate     = baudrate;
	UartHandle.Init.WordLength   = UART_WORDLENGTH_8B;
	UartHandle.Init.StopBits     = UART_STOPBITS_1;
	UartHandle.Init.Parity       = UART_PARITY_NONE;
	UartHandle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
	UartHandle.Init.Mode         = UART_MODE_TX_RX;
	UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;
    
	if(HAL_UART_Init(&UartHandle) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
	* @brief init Timer9 for PWM
	* @param None
	* @retval None
*/
static void Timer9_Init(void){
	TIM_OC_InitTypeDef sConfig;
	uint32_t uwPrescalerValue = 0;
	
	uwPrescalerValue = ((SystemCoreClock /2) / 18000000) - 1;
	
	TimHandle_9.Instance = TIMx;
	
	TimHandle_9.Init.Prescaler     = uwPrescalerValue;
	TimHandle_9.Init.Period        = PERIOD_VALUE;
	TimHandle_9.Init.ClockDivision = 0;
	TimHandle_9.Init.CounterMode   = TIM_COUNTERMODE_UP;
	if(HAL_TIM_PWM_Init(&TimHandle_9) != HAL_OK)
	{
		/* Initialization Error */
		Error_Handler();
	}
	
	sConfig.OCMode			= TIM_OCMODE_PWM1;
	sConfig.OCPolarity		= TIM_OCPOLARITY_HIGH;
	sConfig.OCNPolarity		= TIM_OCNPOLARITY_HIGH;
	sConfig.OCNIdleState	= TIM_OCNIDLESTATE_RESET;
	sConfig.OCIdleState		= TIM_OCIDLESTATE_RESET;
	sConfig.OCFastMode		= TIM_OCFAST_DISABLE;
	
	sConfig.Pulse = PULSE1_VALUE;  
	if(HAL_TIM_PWM_ConfigChannel(&TimHandle_9, &sConfig, TIM_CHANNEL_1) != HAL_OK)
	{
		/* Configuration Error */
		Error_Handler();
	}
	
	if(HAL_TIM_PWM_Start(&TimHandle_9, TIM_CHANNEL_1) != HAL_OK)
	{
		/* Starting Error */
		Error_Handler();
	}
}

/**
  * @brief init timer_4
  * @param None
  * @retval None
  */
static void timer_4_init(void){
	RCC_ClkInitTypeDef sClokConfig;
	uint32_t uwTimclock, uwAPB1Prescaler = 0;
	uint32_t uwPrescalerValue = 0;
	uint32_t pFLatency;
	
	HAL_RCC_GetClockConfig(&sClokConfig, &pFLatency);
	
	uwAPB1Prescaler = sClokConfig.APB1CLKDivider;
	if (uwAPB1Prescaler == 0) 
	{
		uwTimclock = HAL_RCC_GetPCLK1Freq();
	}
	else
	{
		uwTimclock = 2 * HAL_RCC_GetPCLK1Freq();
	}
	
	uwPrescalerValue = (uint32_t) ((uwTimclock / 20000) - 1);
	
	TimHandle_4.Instance = TIMx_4;
	
	TimHandle_4.Init.Period = 20000 - 1;
	TimHandle_4.Init.Prescaler = uwPrescalerValue;
	TimHandle_4.Init.ClockDivision = 0;
	TimHandle_4.Init.CounterMode = TIM_COUNTERMODE_UP;
	if(HAL_TIM_Base_Init(&TimHandle_4) != HAL_OK)
	{
		/* Initialization Error */
		Error_Handler();
	}
}

/**
  * @brief init timer_5 at 20KHz
  * @param None
  * @retval None
  */
static void timer_5_init(void){
	RCC_ClkInitTypeDef sClokConfig;
	uint32_t uwTimclock, uwAPB1Prescaler = 0;
	uint32_t uwPrescalerValue = 0;
	uint32_t pFLatency;
	
	HAL_RCC_GetClockConfig(&sClokConfig, &pFLatency);
	
	uwAPB1Prescaler = sClokConfig.APB1CLKDivider;
	if (uwAPB1Prescaler == 0) 
	{
		uwTimclock = HAL_RCC_GetPCLK1Freq();
	}
	else
	{
		uwTimclock = 2 * HAL_RCC_GetPCLK1Freq();
	}
	
	uwPrescalerValue = (uint32_t) ((uwTimclock / 20000) - 1);
	
	TimHandle_5.Instance = TIMx_5;
	
	TimHandle_5.Init.Period = 70 - 1;
	TimHandle_5.Init.Prescaler = uwPrescalerValue;
	TimHandle_5.Init.ClockDivision = 0;
	TimHandle_5.Init.CounterMode = TIM_COUNTERMODE_UP;
	if(HAL_TIM_Base_Init(&TimHandle_5) != HAL_OK)
	{
		/* Initialization Error */
		Error_Handler();
	}
}

/******************************************************************************/

/**
	* @brief  Retargets the C library printf function to the USART (GNU)
	* @param  None
	* @retval None
*/
int __io_putchar(int ch){
	/* Place your implementation of fputc here */
	/* e.g. write a character to the USART */
	
	Debug_ITMDebugOutputChar(ch); // SWV
	
	return(ch);
}

//******************************************************************************

/**
	* @brief  Retargets the C library scanf function to the USART (GNU)
	* @param  None
	* @retval None
*/
int __io_getchar(void){
	/* Place your implementation of fgetc here */
	/* e.g. read a character from the USART */
	
	return((int)-1);
}

//******************************************************************************

#ifdef  USE_FULL_ASSERT
	/**
		* @brief  Reports the name of the source file and the source line number
		*         where the assert_param error has occurred.
		* @param  file: pointer to the source file name
		* @param  line: assert_param error line source number
		* @retval None
	*/
	void assert_failed(uint8_t* file, uint32_t line){
		printf("Wrong parameters value: file %s on line %d\r\n", file, line);
		
		while(1); /* Infinite loop */
	}
#endif

//******************************************************************************
