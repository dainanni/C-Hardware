#include <string.h>
#include <math.h>
#include <stdio.h>
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
#include "timer.h"
#include "uart_if.h"
#include "timer_if.h"
#include "pinmux.h"

#define APPLICATION_VERSION     "1.1.1"

#define SPI_IF_BIT_RATE  1000000
#define TR_BUFF_SIZE     100

#define BUTTON_ONE      49000 //
#define BUTTON_TWO      50000 //
#define BUTTON_THREE    51000 //
#define BUTTON_FOUR     52000 //
#define BUTTON_FIVE     53000 //
#define BUTTON_SIX      54000 //
#define BUTTON_SEVEN    55000 //
#define BUTTON_EIGHT    56000 //
#define BUTTON_NINE     57000 //
#define BUTTON_ZERO     0 //
#define BUTTON_CHUP     35000 //pound
#define BUTTON_CHDOWN   42000 //star
#define BUTTON_DELAY    66666666

#define X_CHAR 6
#define Y_CHAR 8

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

typedef struct PinSetting
{
    unsigned long port;
    unsigned int pin;
} PinSetting;
static PinSetting adc_cs = { .port = GPIOA3_BASE, .pin = 0x80 };

static void BoardInit(void)
{

#ifndef USE_TIRTOS

#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long) &g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif

    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}
// GLOBAL VARS
int timerflag = 0;
int ADC_value = 0;
int i = 0;
long int button_pressed = 0;
int N = 410;                       // block size
int samples[410];       // buffer to store N samples
long int power_all[8];       // array to store calculated power of 8 frequencies
long int coeff[8] = { 31548, 31281, 30951, 30556, 29143, 28360, 27408, 26258 }; // array to store the calculated coefficients
int f_tone[8] = { 697, 770, 852, 941, 1209, 1336, 1477, 1633 }; // frequencies of rows & columns
int new_dig; // flag set when inter-digit interval (pause) is detected
char decoded_letter;
int sampleIndex = 0;
int sampleFull = 0;
int sampleReady = 0;

//GLOBAL VARS

void processADC(void);

static void UARTInt(void)
{

}

int getADC(void)
{
    int value;

    unsigned char c1 = 0, c2 = 0;
    GPIOPinWrite(adc_cs.port, adc_cs.pin, 0x00);

    MAP_SPICSEnable(GSPI_BASE);

    SPITransfer(GSPI_BASE, 0, &c1, 1, (SPI_CS_ENABLE | SPI_CS_DISABLE));
    SPITransfer(GSPI_BASE, 0, &c2, 1, (SPI_CS_ENABLE | SPI_CS_DISABLE));
    MAP_SPICSDisable(GSPI_BASE);
    GPIOPinWrite(adc_cs.port, adc_cs.pin, 0xff);

    c1 = c1 << 3;
    c2 = c2 >> 3;
    unsigned int temp1 = c1 << 2;
    unsigned int temp2 = c2;
    value = temp1 | temp2;
    value -= 388; // removing dc bias

    return value;
}

void TimerBaseIntHandler(void)
{
    sampleFull = 0;
    Timer_IF_InterruptClear(TIMERA0_BASE);

    sampleReady = 1;
    sampleIndex++;

    if(sampleIndex == N)
    {
        sampleFull = 1;
        sampleIndex = 0;
        Timer_IF_DeInit(TIMERA0_BASE, TIMER_A);
    }

}

long int goertzel(int sample[], long int coeff, int N)
{
    int Q, Q_prev, Q_prev2;
    long prod1, prod2, prod3, power;

    Q_prev = 0;
    Q_prev2 = 0;
    power = 0;

    for (i = 0; i < N; i++)
    {
        Q = (sample[i]) + ((coeff * Q_prev) >> 14) - (Q_prev2);
        Q_prev2 = Q_prev;
        Q_prev = Q;
    }

    prod1 = ((long) Q_prev * Q_prev);
    prod2 = ((long) Q_prev2 * Q_prev2);
    prod3 = ((long) Q_prev * coeff) >> 14;
    prod3 = (prod3 * Q_prev2);

    power = ((prod1 + prod2 - prod3)) >> 8;

    return power;
}

char analyzeGoertzel(void)
{
    int row, col, max_power;

    max_power = 0;

    char row_col[4][4] = {
                            {'1', '2', '3', 'A'},
                            {'4', '5', '6', 'B'},
                            {'7', '8', '9', 'C'},
                            {'*', '0', '#', 'D'}
                        };

    for (i = 0; i < 4; i++)
    {
        if (power_all[i] > max_power)
        {
            max_power = power_all[i];
            row = i;
        }
    }

    max_power = 0;

    for (i = 4; i < 8; i++)
    {
        if (power_all[i] > max_power)
        {
            max_power = power_all[i];
            col = i;
        }
    }

    int noiseCheckRow = 0;
    int noiseCheckCol = 0;

    for(i = 0; i < 4; i++)
        if(power_all[i] > 100000)
            noiseCheckRow++;

    for(i = 4; i < 8; i++)
        if(power_all[i] > 100000)
            noiseCheckCol++;

    if (power_all[col] < 1000 && power_all[row] < 1000)
    {
        new_dig = 1;
    }

    if ((power_all[col] > 1000 && power_all[row] > 1000) && (new_dig == 1) && (noiseCheckRow == 1) && (noiseCheckCol == 1))
    {
        new_dig = 0;
        return row_col[row][col - 4];
    }

    return '$';
}

void processADC(void)
{
    int j;

    for (j = 0; j < 8; j++)
        power_all[j] = goertzel(samples, coeff[j], N);

    decoded_letter = analyzeGoertzel();

}

void main()
{
    unsigned long ulStatus;

    BoardInit();
    PinMuxConfig();

    InitTerm();
    ClearTerm();

    MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_GSPI);

    MAP_SPIReset(GSPI_BASE);
    MAP_SPIConfigSetExpClk(GSPI_BASE, MAP_PRCMPeripheralClockGet(PRCM_GSPI),
    SPI_IF_BIT_RATE,
                           SPI_MODE_MASTER, SPI_SUB_MODE_0, (SPI_SW_CTRL_CS |
                           SPI_4PIN_MODE |
                           SPI_TURBO_OFF |
                           SPI_CS_ACTIVEHIGH |
                           SPI_WL_8));
    MAP_SPIEnable(GSPI_BASE);

    Adafruit_Init();
    fillScreen(GREEN);

    MAP_UARTEnable(UARTA1_BASE);
    MAP_UARTFIFOEnable(UARTA1_BASE);
    MAP_UARTConfigSetExpClk(UARTA1_BASE,
    MAP_PRCMPeripheralClockGet(PRCM_UARTA1),
                            UART_BAUD_RATE,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                            UART_CONFIG_PAR_NONE));
    MAP_UARTIntRegister(UARTA1_BASE, UARTInt);
    ulStatus = MAP_UARTIntStatus(UARTA1_BASE, false);
    MAP_GPIOIntClear(UARTA1_BASE, ulStatus);
    MAP_UARTIntEnable(UARTA1_BASE, (UART_INT_RX | UART_INT_RT));


    Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);
    Timer_IF_IntSetup(TIMERA0_BASE, TIMER_A, TimerBaseIntHandler);
    Timer_IF_Start(TIMERA0_BASE, TIMER_A, 5000);

    Timer_IF_Init(PRCM_TIMERA1, TIMERA1_BASE, TIMER_CFG_PERIODIC, TIMER_A, 0);

    int x = 0;
    int y = 0;

    while (1)
    {
        while (!sampleFull && !sampleReady)
            ;

        if(sampleReady)
        {
            sampleReady = 0;
            samples[sampleIndex] = getADC();
        }

        if (sampleFull)
        {  // clear flag


            processADC();

            if (decoded_letter != '$')
            {
                printf("char received = %c\r\n", decoded_letter);
                drawChar(x, y, decoded_letter, WHITE, BLACK, 1);
                x += X_CHAR;

                if(x > 124)
                {
                    x = 0;
                    y += Y_CHAR;
                }
            }

            Timer_IF_Init(PRCM_TIMERA0, TIMERA0_BASE, TIMER_CFG_PERIODIC,
            TIMER_A,
                          0);
            Timer_IF_IntSetup(TIMERA0_BASE, TIMER_A, TimerBaseIntHandler);
            Timer_IF_Start(TIMERA0_BASE, TIMER_A, 5000);

        }
    }


}
