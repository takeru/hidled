#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "usbdrv.h"

uchar recv_buf[8];
void dataReceived(void);

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
        dataReceived();
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

uchar led_duty_portc[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
uchar led_duty_portb[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
uchar led_duty_portd7   = 0x00;
void dataReceived(void)
{
    uchar i;
    for(i=0; i<6; i++){
        uchar v = recv_buf[i/2];
        if(i%2==0){
            v = (v & 0xF0) >> 4;
        }else{
            v = (v & 0x0F);
        }
        led_duty_portc[5-i] = v*v;
    }
    for(i=0; i<6; i++){
        uchar v = recv_buf[3+i/2];
        if(i%2==0){
            v = (v & 0xF0) >> 4;
        }else{
            v = (v & 0x0F);
        }
        led_duty_portb[5-i] = v*v;
    }
    led_duty_portd7 = recv_buf[6] >> 4;
}

void updateLED(uchar counter)
{
    uchar i;
    // LED PORTB 0,1,2 red
    //           3,4,5 yellow
    //     PORTC 0,1,2 green
    //           3,4,5 blue
    //     PORTD 7     white

    // recv_buf[0] => PORTC 5,4
    // recv_buf[1] => PORTC 3,2
    // recv_buf[2] => PORTC 1,0
    uint8_t portc = 0;
    for(i=0; i<6; i++){
        if(counter < led_duty_portc[i]){
          portc |= _BV(i);
        }
    }
    PORTC = portc;

    // recv_buf[3] => LED PORTB 5,4
    // recv_buf[4] => LED PORTB 3,2
    // recv_buf[5] => LED PORTB 1,0
    uint8_t portb = 0;
    for(i=0; i<6; i++){
        if(counter < led_duty_portb[i]){
          portb |= _BV(i);
        }
    }
    PORTB = portb;

    // recv_buf[6] => LED PORTD 7
    if(counter < led_duty_portd7){
      PORTD |=   _BV(7);
    }else{
      PORTD &= ~ _BV(7);
    }

    /*
    // SLOW
    volatile uint8_t *ports[3] = {&PORTC, &PORTB, &PORTD};
    for(i=0; i<13; i++){
        uchar v = recv_buf[i/2];
        if(i%2==0){
            v = (v & 0xF0) >> 4;
        }else{
            v = (v & 0x0F);
        }
        volatile uint8_t *port = ports[i/6];
        uchar bit = 5-(i%6);
        if(12<=i){ bit = 7; }
        if(counter%256 < v*v){
          *port |=   _BV(bit);
        }else{
          *port &= ~ _BV(bit);
        }
    }
    */
}

int main(void)
{
    uchar i;
    wdt_enable(WDTO_1S);

    // beep
    DDRD = _BV(PORTD5);
    for(i=0; i<10; i++){
        PORTD |= _BV(PORTD5);
        _delay_ms(1);
        PORTD &= ~ _BV(PORTD5);
        _delay_ms(1);
    }

    DDRB = 0x3F; // 0011 1111
    DDRC = 0x3F; // 0011 1111
    DDRD = 0x80; // 1000 0000

    usbInit();
    usbDeviceDisconnect();
    for(i=0; i<150; i++){
        wdt_reset();
        _delay_ms(2);
    }
    usbDeviceConnect();

    sei();
    for(i=0;;i++){
        wdt_reset();
        usbPoll();
        updateLED(i);
    }
    return 0;
}
