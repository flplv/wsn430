#include <io.h>
#include <signal.h>

/* Project includes */
#include "clock.h"
#include "leds.h"
#include "uart1.h"
#include "supply-control.h"
#include "ina209.h"


#define SYNC_BYTE 0x80
#define END_BYTE 0xFF
#define CMD_PING 0x01
#define CMD_DC_ON 0x02
#define CMD_DC_OFF 0x03
#define CMD_BATT_ON 0x04
#define CMD_BATT_OFF 0x05
#define CMD_MEAS_DC 0x06
#define CMD_MEAS_BATT 0x07


#define CMD_FRAME_LEN 3
#define RESP_FRAME_LEN 7

/* Function Protypes */
static void setupHardware( void );
static uint16_t char_received(uint8_t c);

typedef union {
	char data[CMD_FRAME_LEN];
	struct {
		char sync;
		char cmd;
		char end;
	};
} cmd_frame_t;

typedef union {
	char data[RESP_FRAME_LEN];
	struct {
		char sync;
		char cmd;
		char payload[4];
		char end;
	};
} resp_frame_t;

cmd_frame_t cmd;
resp_frame_t resp;

int cmd_ready = 0;

/**
 * The main function.
 */
int main( void )
{
    uint16_t vdc, idc, vbatt, ibatt;
    
    /* Setup the hardware. */
    setupHardware();
    
    while (1)
    {
        // low power until command received
        LPM0;
        if (cmd_ready)
        {
            cmd_ready = 0;
            int j;
            for (j=0; j<RESP_FRAME_LEN; j++)
                resp.data[j] = 0;
            
            resp.sync = SYNC_BYTE;
            resp.cmd = cmd.cmd;
            resp.end = END_BYTE;
            
            switch (cmd.cmd) {
                case CMD_PING:
                    resp.payload[0] = 1;
                    break;
                case CMD_DC_ON:
                    set_opennode_main_supply_on();
                    resp.payload[0] = 1;
                    break;
                case CMD_DC_OFF:
                    set_opennode_main_supply_off();
                    resp.payload[0] = 1;
                    break;
                case CMD_BATT_ON:
                    set_opennode_battery_supply_on();
                    resp.payload[0] = 1;
                    break;
                case CMD_BATT_OFF:
                    set_opennode_battery_supply_off();
                    resp.payload[0] = 1;
                    break;
                    
                case CMD_MEAS_DC:
                
                    vdc = ina209_read_dc_voltage();
                    resp.payload[0] = (vdc>>8)&0xFF;
                    resp.payload[1] = vdc&0xFF;
                    idc = ina209_read_dc_current();
                    resp.payload[2] = (idc>>8)&0xFF;
                    resp.payload[3] = idc&0xFF;
                    break;
                    
                case CMD_MEAS_BATT:
                    
                    vbatt = ina209_read_batt_voltage();
                    resp.payload[0] = (vbatt>>8)&0xFF;
                    resp.payload[1] = vbatt&0xFF;
                    ibatt = ina209_read_batt_current();
                    resp.payload[2] = (ibatt>>8)&0xFF;
                    resp.payload[3] = ibatt&0xFF;
                    break;
            }
            
            for (j=0; j<RESP_FRAME_LEN; j++)
            {
                uart1_putchar(resp.data[j]);
            }
            
        }
        
    }
    
    
    return 0;
}


uint16_t char_received(uint8_t c)
{
    static int ix = 0;
    
    if ( (ix == 0) && (c == SYNC_BYTE) && (cmd_ready == 0) )
    {
        cmd.data[ix] = c;
        ix++;
        return 0;
    } else if ( ix == 1 )
    {
        cmd.data[ix] = c;
        ix++;
        return 0;
    } else if ( (ix == 2) && (c == END_BYTE) ) {
        
        cmd.data[ix] = c;
        cmd_ready = 1;
        ix = 0;
        return 1;
    } else {
        ix = 0;
        return 0;
    }
    
    return 0;
}

/**
 * Initialize the main hardware parameters.
 */
static void setupHardware( void )
{
    /* Stop the watchdog timer. */
    WDTCTL = WDTPW + WDTHOLD;
    
    /* Setup MCLK 8MHz and SMCLK 1MHz */
    set_mcu_speed_xt2_mclk_8MHz_smclk_1MHz();
    
    
    /* Initialize the LEDs */
    LEDS_INIT();
    LEDS_ON();
    
    // setup IOs for relays control
    setup_senslabgw_ios();
    set_opennode_main_supply_off();
    set_opennode_battery_supply_off();
    
    // setup uart
    uart1_init(UART1_CONFIG_1MHZ_115200);
    uart1_register_callback(char_received);
    
    // setup ina209
    ina209_init();
    
    /* Enable Interrupts */
    eint();
}
