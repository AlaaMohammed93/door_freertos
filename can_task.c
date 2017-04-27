#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/systick.h"
#include "driverlib/rom.h"
#include "utils/uartstdio.h"
#include "inc/hw_can.h"
#include "driverlib/can.h"
#include "inc/hw_gpio.h"
#include "inc/hw_types.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

//*****************************************************************************
//
// The stack size for the CAN sending task.
//
//*****************************************************************************
#define CANTASKSTACKSIZE        128         // Stack size in words

//*****************************************************************************
//
// The item size and queue size for the LED message queue.
//
//*****************************************************************************

#define CAN_ITEM_SIZE           sizeof(uint32_t)
#define CAN_QUEUE_SIZE          5



volatile bool g_bErrFlag = 0;

void CANTask(void);
//*****************************************************************************
//
// The queue that holds messages sent to the CAN task.
//
//*****************************************************************************
xQueueHandle g_pCANQueue;



void CANIntHandler(void)
{
    uint32_t ui32Status;

    // Read the CAN interrupt status to find the cause of the interrupt

    ui32Status = CANIntStatus(CAN0_BASE, CAN_INT_STS_CAUSE);

    // If the cause is a controller status interrupt, then get the status

    if(ui32Status == CAN_INT_INTID_STATUS)
    {

        // Read the controller status.  This will return a field of status error bits that can indicate various errors.
        //Error processing is not done in this example for simplicity.  Refer to the API documentation for details about the error status bits.
        // The act of reading this status will clear the interrupt.  If the CAN peripheral is not connected to a CAN bus with other CAN devices
        // present, then errors will occur and will be indicated in the controller status.

        ui32Status = CANStatusGet(CAN0_BASE, CAN_STS_CONTROL);

        // Set a flag to indicate some errors may have occurred.

        g_bErrFlag = 1;
    }

    // Check if the cause is message object 1, which what we are using for sending messages.

    else if(ui32Status == 1)
    {
        // Getting to this point means that the TX interrupt occurred on message object 1, and the message TX is complete.
        //Clear the message object interrupt.

        CANIntClear(CAN0_BASE, 1);

        // Since the message was sent, clear any error flags.

        g_bErrFlag = 0;
    }
}

uint32_t CANTaskInit(void)
{
    // CAN0 is used with RX and TX pins on port E4 and E5.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    // Configure the GPIO pin muxing to select CAN0 functions for these pins.
    //This step selects which alternate function is available for these pins.
    // This is necessary if your part supports GPIO pin function muxing.

    GPIOPinConfigure(GPIO_PE4_CAN0RX);
    GPIOPinConfigure(GPIO_PE5_CAN0TX);

    // Enable the alternate function on the GPIO pins.  The above step selects which alternate function is available.
    // This step actually enables the alternate function instead of GPIO for these pins.
    // TODO: change this to match the port/pin you are using

    GPIOPinTypeCAN(GPIO_PORTE_BASE, GPIO_PIN_4 | GPIO_PIN_5);

    // The GPIO port and pins have been set up for CAN.  The CAN peripheral must be enabled.

    SysCtlPeripheralEnable(SYSCTL_PERIPH_CAN0);
    CANInit(CAN0_BASE);
    CANBitRateSet(CAN0_BASE, SysCtlClockGet(), 500000);

    // Enable interrupts on the CAN peripheral.  This example uses static allocation of interrupt handlers which means the name of the handler
    // is in the vector table of startup code.  If you want to use dynamic allocation of the vector table,
    // then you must also call CANIntRegister() there.

    // CANIntRegister(CAN0_BASE, CANIntHandler); // if using dynamic vectors

    CANIntEnable(CAN0_BASE, CAN_INT_MASTER | CAN_INT_ERROR | CAN_INT_STATUS);

    // Enable the CAN interrupt on the processor (NVIC).

    IntEnable(INT_CAN0);

    // Enable the CAN for operation.

     CANEnable(CAN0_BASE);


     g_pCANQueue = xQueueCreate(CAN_QUEUE_SIZE, CAN_ITEM_SIZE);

        //
        // Create the CAN task.
        //
        if(xTaskCreate(CANTask, (const portCHAR *)"CAN", CANTASKSTACKSIZE, NULL,
                       tskIDLE_PRIORITY + PRIORITY_CAN_TASK, NULL) != pdTRUE)


        {
            return(1);
        }

        //
        // Success.
        //
        return(0);


}

void CANTask(void)
{

    uint32_t ui32MsgData;
    uint8_t *pui8MsgData;

    tCANMsgObject sCANMessage;
    pui8MsgData = (uint8_t *)&ui32MsgData;

    // Set the clocking to run directly from the external crystal/oscillator.
    // TODO: The SYSCTL_XTAL_ value must be changed to match the value of the crystal on your board.

    SysCtlClockSet(SYSCTL_SYSDIV_4|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|SYSCTL_OSC_MAIN);

    // Initialize the message object that will be used for sending CAN messages.
    // The message will be 4 bytes that will contain an incrementing value.  Initially it will be set to 0.

    ui32MsgData = 0;
    sCANMessage.ui32MsgID =60;
    sCANMessage.ui32MsgIDMask = 0;
    sCANMessage.ui32Flags = MSG_OBJ_TX_INT_ENABLE;
    sCANMessage.ui32MsgLen = sizeof(pui8MsgData);
    sCANMessage.pui8MsgData = pui8MsgData;




    while(1)
    {

        if(xQueueReceive(g_pCANQueue, &ui32MsgData, 0) == pdPASS)
        {
            sCANMessage.pui8MsgData = pui8MsgData;
        }

        CANMessageSet(CAN0_BASE, 1, &sCANMessage, MSG_OBJ_TYPE_TX);

        //delay half second
        SysCtlDelay(8000000 / 3);

        if(g_bErrFlag)
        {
           UARTprintf(" Error - cable connected?\n");
        }
        else
        {
            // If no errors then print the count of message sent
            UARTprintf("Sending msg: 0x%02X %02X %02X %02X \n", pui8MsgData[0], pui8MsgData[1], pui8MsgData[2], pui8MsgData[3]);
        }
    }
}


