#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "usbdrv.h"

uchar recv_buf[8];
uchar counter = 0;

/*
typedef struct usbRequest{
    uchar       bmRequestType;
    uchar       bRequest;
    usbWord_t   wValue;
    usbWord_t   wIndex;
    usbWord_t   wLength;
}usbRequest_t;
*/

PROGMEM const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)

    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)

    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)

    0xc0                           // END_COLLECTION
};


/* You need to implement this function ONLY if you provide USB descriptors at
 * runtime (which is an expert feature). It is very similar to
 * usbFunctionSetup() above, but it is called only to request USB descriptor
 * data. See the documentation of usbFunctionSetup() above for more info.
 */
/*
usbMsgLen_t usbFunctionDescriptor(struct usbRequest_t *rq)
{
}
*/


/* This function is called when the driver receives a SETUP transaction from
 * the host which is not answered by the driver itself (in practice: class and
 * vendor requests). All control transfers start with a SETUP transaction where
 * the host communicates the parameters of the following (optional) data
 * transfer. The SETUP data is available in the 'data' parameter which can
 * (and should) be casted to 'usbRequest_t *' for a more user-friendly access
 * to parameters.
 *
 * If the SETUP indicates a control-in transfer, you should provide the
 * requested data to the driver. There are two ways to transfer this data:
 * (1) Set the global pointer 'usbMsgPtr' to the base of the static RAM data
 * block and return the length of the data in 'usbFunctionSetup()'. The driver
 * will handle the rest. Or (2) return USB_NO_MSG in 'usbFunctionSetup()'. The
 * driver will then call 'usbFunctionRead()' when data is needed. See the
 * documentation for usbFunctionRead() for details.
 *
 * If the SETUP indicates a control-out transfer, the only way to receive the
 * data from the host is through the 'usbFunctionWrite()' call. If you
 * implement this function, you must return USB_NO_MSG in 'usbFunctionSetup()'
 * to indicate that 'usbFunctionWrite()' should be used. See the documentation
 * of this function for more information. If you just want to ignore the data
 * sent by the host, return 0 in 'usbFunctionSetup()'.
 *
 * Note that calls to the functions usbFunctionRead() and usbFunctionWrite()
 * are only done if enabled by the configuration in usbconfig.h.
 */
usbMsgLen_t usbFunctionSetup(uchar data[8])
{
    usbRequest_t *rq = (usbRequest_t*)data;

    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){
        if(rq->bRequest == USBRQ_HID_SET_REPORT){
            // host to device
            return USB_NO_MSG; // => usbFunctionWrite
        }
        if(rq->bRequest == USBRQ_HID_GET_REPORT){
            // device to host
            return USB_NO_MSG; // => usbFunctionRead
        }
    }

    return 0;
}

/* This function is called by the driver to provide a control transfer's
 * payload data (control-out). It is called in chunks of up to 8 bytes. The
 * total count provided in the current control transfer can be obtained from
 * the 'length' property in the setup data. If an error occurred during
 * processing, return 0xff (== -1). The driver will answer the entire transfer
 * with a STALL token in this case. If you have received the entire payload
 * successfully, return 1. If you expect more data, return 0. If you don't
 * know whether the host will send more data (you should know, the total is
 * provided in the usbFunctionSetup() call!), return 1.
 * NOTE: If you return 0xff for STALL, 'usbFunctionWrite()' may still be called
 * for the remaining data. You must continue to return 0xff for STALL in these
 * calls.
 * In order to get usbFunctionWrite() called, define USB_CFG_IMPLEMENT_FN_WRITE
 * to 1 in usbconfig.h and return 0xff in usbFunctionSetup()..
 */
uchar usbFunctionWrite(uchar *data, uchar len)
{
    // host to device
    if(len==8){
        memcpy(recv_buf, data, len);
        return 1;
    }else{
        return -1; // STALL
    }
}

/* This function is called by the driver to ask the application for a control
 * transfer's payload data (control-in). It is called in chunks of up to 8
 * bytes each. You should copy the data to the location given by 'data' and
 * return the actual number of bytes copied. If you return less than requested,
 * the control-in transfer is terminated. If you return 0xff, the driver aborts
 * the transfer with a STALL token.
 * In order to get usbFunctionRead() called, define USB_CFG_IMPLEMENT_FN_READ
 * to 1 in usbconfig.h and return 0xff in usbFunctionSetup()..
 */
uchar usbFunctionRead(uchar *data, uchar len)
{
    // device to host
    if(len==8){
        //memset(data, 0, len);
        memcpy(data, recv_buf, len);
        data[7] = counter++;
        return len;
    }else{
        return -1; // STALL
    }
}

/* This function is called by the driver when data is received on an interrupt-
 * or bulk-out endpoint. The endpoint number can be found in the global
 * variable usbRxToken. You must define USB_CFG_IMPLEMENT_FN_WRITEOUT to 1 in
 * usbconfig.h to get this function called.
 */
 /*
void usbFunctionWriteOut(uchar *data, uchar len)
{
    usbDisableAllRequests();
    // ...
    usbEnableAllRequests();
}
*/

static void hardwareInit(void)
{
    uchar i;
    USB_CFG_IOPORT = (uchar)~((1<<USB_CFG_DMINUS_BIT)|(1<<USB_CFG_DPLUS_BIT));

    usbDeviceDisconnect();
    for(i=0; i<10; i++){
        wdt_reset();
        _delay_ms(10);
    }
    usbDeviceConnect();
}

int main(void)
{
    wdt_enable(WDTO_1S);
    hardwareInit();
    usbInit();

    sei();
    for(;;){
        wdt_reset();
        usbPoll();
    }
    return 0;
}
