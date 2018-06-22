#include <string.h>

#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "prcm.h"
#include "uart.h"
#include "gpio.h"
#include "interrupt.h"
#include "Adafruit_GFX.h"
#include "Adafruit_OLED.h"
#include "Adafruit_SSD1351.h"
#include "glcdfont.h"
#include "test.h"

#include "uart_if.h"
#include "pinmux.h"


#define APPLICATION_VERSION     "1.1.1"
//*****************************************************************************
//
// Application Master/Slave mode selector macro
//
// MASTER_MODE = 1 : Application in master mode
// MASTER_MODE = 0 : Application in slave mode
//
//*****************************************************************************

#define SPI_IF_BIT_RATE  1000000
#define TR_BUFF_SIZE     100

#define MASTER_MSG       "This is CC3200 SPI Master Application\n\r"
#define SLAVE_MSG        "This is CC3200 SPI Slave Application\n\r"

//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************


#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif
//*****************************************************************************
//                 GLOBAL VARIABLES -- End
//*****************************************************************************

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void
BoardInit(void)
{
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

//*****************************************************************************
//
//! Main function for spi demo application
//!
//! \param none
//!
//! \return None.
//
//*****************************************************************************

void testProtocol(void)
{
    fillScreen(BLACK);

    int i;
    int x = 0, y = 0;
    for(i = 0; i<1276; i++)
    {
        if(x > 124)
        {
            x = 0;
            y += 9;
        }

        if(y > 119)
        {
            delay(100);
            fillScreen(BLACK);
            x = 0;
            y = 0;
        }

        drawChar(x, y, font[i], GREEN, BLACK, 1);
        x += 6;

    }
    delay(100);

    fillScreen(BLACK);
    setCursor(0, 0);
    Outstr("Hello World!");
    delay(250);

    fillRect(0, 0, 128, 16, MAGENTA);
    fillRect(0, 16, 128, 16, CYAN);
    fillRect(0, 32, 128, 16, BLUE);
    fillRect(0, 48, 128, 16, GREEN);
    fillRect(0, 64, 128, 16, BLACK);
    fillRect(0, 80, 128, 16, YELLOW);
    fillRect(0, 96, 128, 16, WHITE);
    fillRect(0, 112, 128, 16, RED);
    delay(250);

    fillScreen(BLACK);
    for(i = 0; i <= 128; i++)
    {
        drawFastHLine(0, i, 16, MAGENTA);
        drawFastHLine(16, i, 16, CYAN);
        drawFastHLine(32, i, 16, BLUE);
        drawFastHLine(48, i, 16, GREEN);
        drawFastHLine(64, i, 16, BLACK);
        drawFastHLine(80, i, 16, YELLOW);
        drawFastHLine(96, i, 16, WHITE);
        drawFastHLine(112, i, 16, RED);
    }
    delay(250);

    fillScreen(BLACK);
    testlines(BLUE);
    delay(250);

    testfastlines(BLACK, GREEN);
    delay(250);

    testdrawrects(RED);
    delay(250);

    testfillrects(RED, GREEN);
    delay(250);

    fillScreen(BLACK);
    testfillcircles(35, BLUE);
    delay(250);

    fillScreen(BLACK);
    testdrawcircles(35, GREEN);
    delay(250);

    testroundrects();
    delay(250);

    testtriangles();
    delay(250);
}

void main()
{

    BoardInit();
    PinMuxConfig();
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);

    MAP_PRCMPeripheralReset(PRCM_GSPI);

    MAP_SPIReset(GSPI_BASE);
    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                         SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                         (SPI_SW_CTRL_CS |
                         SPI_4PIN_MODE |
                         SPI_TURBO_OFF |
                         SPI_CS_ACTIVEHIGH |
                         SPI_WL_8));

    MAP_SPIEnable(GSPI_BASE);
    Adafruit_Init();
    while(1)
    {
        testProtocol();
    }

}

