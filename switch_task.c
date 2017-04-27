#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "drivers/buttons.h"
#include "utils/uartstdio.h"
#include "switch_task.h"
#include "can_task.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

//*****************************************************************************
//
// The stack size for the display task.
//
//*****************************************************************************
#define SWITCHTASKSTACKSIZE        128         // Stack size in words




extern xQueueHandle g_pCANQueue;
//extern xSemaphoreHandle g_pUARTSemaphore;

//*****************************************************************************
//
// This task reads the buttons' state and passes this information to CANTask.
//
//*****************************************************************************


void InitButtons (void)
{

    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) = 0xFF;

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);                                                 // Enable port F
    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_4);                                           // Init PF4 as input
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);     // Enable weak pullup resistor for PF4

    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_0);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);



}


static void
SwitchTask(void *pvParameters)
{
    portTickType ui16LastTime;
    uint32_t ui32SwitchDelay = 25;
    uint8_t ui8CurButtonState, ui8PrevButtonState;

    uint32_t ui32MsgData = 0;


    volatile uint32_t g_ui32MsgCount = 0;

    bool SwOnePressed = true;
    bool SwTwoPressed = true;

    ui8CurButtonState = 0;
    ui8PrevButtonState = 0;

    //
    // Get the current tick count.
    //
    ui16LastTime = xTaskGetTickCount();

    //
    // Loop forever.
    //
    while(1)
    {
        //
        // Poll the debounced state of the buttons.
        //
       ui8CurButtonState = ButtonsPoll(0, 0);

        //
        // Check if previous debounced state is equal to the current state.
        //
        if(ui8CurButtonState != ui8PrevButtonState)
        {
            ui8PrevButtonState = ui8CurButtonState;

            //
            // Check to make sure the change in state is due to button press
            // and not due to button release.
            //

            if(GPIOIntStatus(GPIO_PORTF_BASE, false) & GPIO_PIN_4)
            {
/////////////////////////////////////////////////////////////////
                if(SwOnePressed == false)
                {
                    ui32MsgData &= 0x0000ffff;
                    GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_FALLING_EDGE);     // Configure PF4 for falling edge trigger

                    SwOnePressed = true;
                }
                else if (SwOnePressed == true)
                {
                    ui32MsgData |= 0xffff0000;
                    GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_4, GPIO_RISING_EDGE);       // Configure PF for rising edge trigger

                    SwOnePressed = false;
                }
             /////////////////////////////////////////////////////////
        }
        else if(GPIOIntStatus(GPIO_PORTF_BASE, false) & GPIO_PIN_0)
        {
            ////////////////////////////////////////////////
            if(SwTwoPressed == false)
            {
                ui32MsgData &= 0xffff0000;
                GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_FALLING_EDGE);     // Configure PF0 for falling edge trigger

                SwTwoPressed = true;
            }
            else if (SwTwoPressed == true)
            {
                ui32MsgData |= 0x0000ffff;
                GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_0, GPIO_RISING_EDGE);          // Configure PF0 for rising edge trigger

                SwTwoPressed = false;
            }
                    ////////////////////////////////////////////////
        }
                //
                // Pass the value of the button pressed to LEDTask.
                //
        if(xQueueSend(g_pCANQueue, &ui32MsgData, portMAX_DELAY) != pdPASS)
        {
                    //
                    // Error. The queue should never be full. If so print the
                    // error message on UART and wait for ever.
                    //
            UARTprintf("\nQueue full. This should never happen.\n");
            while(1)
            {
            }
         }
      }


        //
        // Wait for the required amount of time to check back.
        //
        vTaskDelayUntil(&ui16LastTime, ui32SwitchDelay / portTICK_RATE_MS);
    }
}

//*****************************************************************************
//
// Initializes the switch task.
//
//*****************************************************************************
uint32_t
SwitchTaskInit(void)
{
    //
    // Unlock the GPIO LOCK register for Right button to work.
    //
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) = 0xFF;

    //
    // Initialize the buttons
    //
    InitButtons();



    //
    // Create the switch task.
    //
    if(xTaskCreate(SwitchTask, (const portCHAR *)"Switch",
                   SWITCHTASKSTACKSIZE, NULL, tskIDLE_PRIORITY +
                   PRIORITY_SWITCH_TASK, NULL) != pdTRUE)
    {
        return(1);
    }

    //
    // Success.
    //
    return(0);
}
