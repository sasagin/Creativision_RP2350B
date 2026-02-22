//This is from 
/*****************************************************************************
/* https://sourceforge.net/projects/creativisionemulator/files/
/***************************************************************************** */
//partially

#include "joypads.h"

/* PIA */
#define PA0 1
#define PA1 2
#define PA2 3
#define PA3 4

#define PIA_INPUT    0
#define PIA_OUTALL 0xff
#define PIA_OUTHN  0xf0
#define PIA_OUTLN  0x0f
#define PIA_CLOAD  0x7f
#define PIA_CSAVE  0xff
#define PIA_KEY0   0xf7
#define PIA_KEY1   0xfb
#define PIA_KEY2   0xfd
#define PIA_KEY3   0xfe
#define PIA_PDR    4
#define PIA_DDR    0
#define PLAY_BUFFER 2048

typedef struct {
    int PDR;  /* Peripheral Data Register */
    int DDR;  /* Data Direction Register */
    int CR;   /* Control Register */
    unsigned long long int prev_cycles;
} M6821;

/* PIA 0 and 1 */
M6821 pia0 = {0};
M6821 pia1 = {0};

unsigned char KEYBD[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };
unsigned char JOYSM[2] = { 255, 255 };

unsigned long long int total_cycles;  // for PIA

/**
 * PIA_Write
 *
 * Called from cpu on access to memory in the range $1000 - $1FFF
 */
void PIA_Write(uint16_t addr, uint8_t data)
{

    switch (addr & 3) {
    case 0: /* PIA 0 DATA PORT */
        if ( pia0.CR & PIA_PDR) {
            if ( pia0.DDR == PIA_OUTALL ) {
                /* This should only happen when the cassette is writing */
                // CSAVE(data);
                pia0.PDR = data;
                pia0.CR = (pia0.CR & 0x3f) | ((pia0.CR & 3) << 6);
            } else {
                /* Input */
                pia0.PDR = data;
            }
        } else
            pia0.DDR = data;
        break;

    case 1: /* PIA 0 CONTROL */
        pia0.CR = (data & 0x3f);
        break;

    case 2: /* PIA 1 DATA PORT */
        if (pia1.CR & PIA_PDR) {
            if (pia1.DDR == PIA_OUTALL) {
                /* Store for retrieval */
                pia1.PDR = data;

                /* Output to SN76489 */
                #define AUD_W       41
                gpio_put(AUD_W,0);              // SN76496SPWrite(0,data);
                sleep_us(18);
                pia1.prev_cycles = total_cycles;

                /* Set IRQ as complete */
                pia1.CR = (pia1.CR & 0x3f) | ( ( pia1.CR & 3) << 6);
            }
            else
                pia1.PDR = data;
        } else {
            pia1.DDR = data;
        }
        break;

    case 3: /* PIA 1 CONTROL */
        pia1.CR = data & 0x3f;
        break;
    }
}

/**
 * PIA_Read
 *
 * Updated to reflect the disassembly of the PIA diagnostic cart
 */
unsigned char PIA_Read(uint16_t addr)
{
    switch( addr & 3 )
    {
    case 0: /* PIA-0 Data Port */

        /* Is the PDR selected */
        if (pia0.CR & PIA_PDR)
        {
            /* Which device was requested? */
            // if (pia0.DDR == PIA_CLOAD)
            //     // pia0.PDR = CLOAD();

            if (!( pia0.DDR & ~pia0.PDR ))/* Is it not an input? */
                return 0xff;

            return pia0.PDR;

        }
        /* Request for DDR */
        return pia0.DDR;


    case 1: /* PIA-0 Control Port */
        pia0.CR &= 0x3f;
        return pia0.CR;

    case 2: /* PIA-1 Data Port */

        /* Clear interrupts */
        if (pia1.CR & PIA_PDR)
        {
            /* Fortunately, the mask set in pia0.PDR tells us which device */
            switch(pia0.PDR)
            {
            case 0xf7:      /* Keyboard Mux 8 */
                pia1.PDR = KEYBD[4];
                break;
            case 0xfb:      /* Keyboard Mux 4 */
                pia1.PDR = KEYBD[3];
                break;
            case 0xfd:      /* Keyboard Mux 2 */
                pia1.PDR = KEYBD[2];
                break;
            case 0xfe:      /* Keyboard Mux 1 */
                pia1.PDR = KEYBD[1];
                break;
            default:
                if (pia1.DDR == PIA_OUTALL) {
                    pia1.CR &= 0x3f;
                    return pia1.PDR;
                }

                if (!(pia1.PDR & ~pia1.DDR)) return 0xff;

                return pia1.PDR;
            }

            return pia1.PDR;
        }

        return pia1.DDR;

    case 3: /* PIA-1 Control Port */
        if ( (total_cycles - pia1.prev_cycles) < 36)
            return pia1.CR;

        pia1.CR &= 0x3f;
        return pia1.CR;
    }

    return 0;   /* Will never get here */
}

//convert NES to CV joypad



void convert_nes_to_CV_joypad(uint16_t nes_joy_data1_2){

    for (size_t i = 0; i < 8; i++)
    {
        KEYBD[i] = 255;
    }

    uint8_t nes_joy_data1 = (uint8_t)(nes_joy_data1_2 & 0x00ff);
    uint8_t nes_joy_data2 = (uint8_t)((nes_joy_data1_2>>8) & 0x00ff); 

    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_UP)) == 0) {KEYBD[1] &= ~KEY_1; return;}
    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_DOWN)) == 0) {KEYBD[2] &= ~KEY_2; return;}
    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_LEFT)) == 0) {KEYBD[2] &= ~KEY_3; return;}
    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_RIGHT)) == 0) {KEYBD[2] &= ~KEY_4; return;}
    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_A)) == 0) {KEYBD[2] &= ~KEY_5; return;}
    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_B)) == 0) {KEYBD[2] &= ~KEY_6; return;}        

    if (((nes_joy_data2 & NES_SELECT)||(nes_joy_data2 & NES_UP)) == 0) {KEYBD[4] &= ~KEY_7; return;}
    if (((nes_joy_data2 & NES_SELECT)||(nes_joy_data2 & NES_DOWN)) == 0) {KEYBD[4] &= ~KEY_8; return;} 
    if (((nes_joy_data2 & NES_SELECT)||(nes_joy_data2 & NES_LEFT)) == 0) {KEYBD[4] &= ~KEY_9; return;} 
    if (((nes_joy_data2 & NES_SELECT)||(nes_joy_data2 & NES_RIGHT)) == 0) {KEYBD[4] &= ~KEY_0; return;} 
    if (((nes_joy_data2 & NES_SELECT)||(nes_joy_data2 & NES_A)) == 0) {KEYBD[4] &= ~KEY_RETURN; return;} 
    if (((nes_joy_data2 & NES_SELECT)||(nes_joy_data2 & NES_B)) == 0) {KEYBD[3] &= ~KEY_SPACE; return;} 
 
    if ((nes_joy_data1 & NES_UP) == 0) {KEYBD[1] &= ~UP_BUTTON;}
    if ((nes_joy_data1 & NES_DOWN) == 0) {KEYBD[1] &= ~DOWN_BUTTON;}
    if ((nes_joy_data1 & NES_LEFT) == 0) {KEYBD[1] &= ~LEFT_BUTTON;}
    if ((nes_joy_data1 & NES_RIGHT) == 0) {KEYBD[1] &= ~RIGHT_BUTTON;}
    if ((nes_joy_data1 & NES_B) == 0) {KEYBD[2] &= ~FIRE_BUTTON;}
    if ((nes_joy_data1 & NES_A) == 0) {KEYBD[1] &= ~ARM_BUTTON;}

    if ((nes_joy_data2 & NES_UP) == 0) {KEYBD[3] &= ~UP_BUTTON;}
    if ((nes_joy_data2 & NES_DOWN) == 0) {KEYBD[3] &= ~DOWN_BUTTON;}
    if ((nes_joy_data2 & NES_LEFT) == 0) {KEYBD[3] &= ~LEFT_BUTTON;}
    if ((nes_joy_data2 & NES_RIGHT) == 0) {KEYBD[3] &= ~RIGHT_BUTTON;}
    if ((nes_joy_data2 & NES_B) == 0) {KEYBD[4] &= ~FIRE_BUTTON;}
    if ((nes_joy_data2 & NES_A) == 0) {KEYBD[3] &= ~ARM_BUTTON;}

}