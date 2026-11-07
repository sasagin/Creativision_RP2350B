//This is from 
/*****************************************************************************
 *                                   \ /
 *                        C R E A T I V I S I O N
 *                            E M U L A T O R
 *                                  0.2.2
 *
 *       This emulator was written using Marat Fayzullin's Emulib*/
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
                #define AUD_W       39
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

    uint8_t nes_joy_data1 = (uint8_t)(nes_joy_data1_2 & 0x00ff);
    uint8_t nes_joy_data2 = (uint8_t)((nes_joy_data1_2>>8) & 0x00ff); 

    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_UP)) == 0) {KEYBD[1] &= ~KEY_1; return;}
    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_DOWN)) == 0) {KEYBD[2] &= ~KEY_2; return;}
    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_LEFT)) == 0) {KEYBD[2] &= ~KEY_3; return;}
    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_RIGHT)) == 0) {KEYBD[2] &= ~KEY_4; return;}
    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_A)) == 0) {KEYBD[2] &= ~KEY_5; return;}
    if (((nes_joy_data1 & NES_SELECT)||(nes_joy_data1 & NES_B)) == 0) {KEYBD[2] &= ~KEY_6; return;}        

    if (((nes_joy_data2 & NES_START)||(nes_joy_data2 & NES_UP)) == 0) {KEYBD[4] &= ~KEY_7; return;}
    if (((nes_joy_data2 & NES_START)||(nes_joy_data2 & NES_DOWN)) == 0) {KEYBD[4] &= ~KEY_8; return;} 
    if (((nes_joy_data2 & NES_START)||(nes_joy_data2 & NES_LEFT)) == 0) {KEYBD[4] &= ~KEY_9; return;} 
    if (((nes_joy_data2 & NES_START)||(nes_joy_data2 & NES_RIGHT)) == 0) {KEYBD[4] &= ~KEY_0; return;} 
    if (((nes_joy_data2 & NES_START)||(nes_joy_data2 & NES_A)) == 0) {KEYBD[4] &= ~KEY_RETURN; return;} 
    if (((nes_joy_data2 & NES_START)||(nes_joy_data2 & NES_B)) == 0) {KEYBD[3] &= ~KEY_SPACE; return;} 
 
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

void translate_keys_to_CRV(uint8_t *KEYBD,int i){

    switch (i)
    {
        case 0x04: KEYBD[2] &= ~KEY_A; break;
        case 0x05: KEYBD[2] &= ~KEY_B; break;
		
        case 0x06: KEYBD[2] &= ~KEY_C; break;
        case 0x07: KEYBD[2] &= ~KEY_D; break;
        case 0x08: KEYBD[2] &= ~KEY_E; break;
        case 0x09: KEYBD[2] &= ~KEY_F; break;
        case 0x0A: KEYBD[2] &= ~KEY_G; break;
        case 0x0B: KEYBD[4] &= ~KEY_H; break;
        case 0x0C: KEYBD[4] &= ~KEY_I; break;
        case 0x0D: KEYBD[4] &= ~KEY_J; break;
		
        case 0x0E: KEYBD[4] &= ~KEY_K; break;
        case 0x0F: KEYBD[4] &= ~KEY_L; break;
        case 0x10: KEYBD[4] &= ~KEY_M; break;
        case 0x11: KEYBD[4] &= ~KEY_N; break;   
        case 0x12: KEYBD[4] &= ~KEY_O; break;
		case 0x13: KEYBD[4] &= ~KEY_P; break;	
		case 0x14: KEYBD[2] &= ~KEY_Q; break;
		case 0x15: KEYBD[2] &= ~KEY_R; break;

        case 0x16: KEYBD[2] &= ~KEY_S; break;
        case 0x17: KEYBD[2] &= ~KEY_T; break;
        case 0x18: KEYBD[4] &= ~KEY_U; break;
        case 0x19: KEYBD[2] &= ~KEY_V; break;   
        case 0x1A: KEYBD[2] &= ~KEY_W; break;
		case 0x1B: KEYBD[2] &= ~KEY_X; break;	
		case 0x1C: KEYBD[4] &= ~KEY_Y; break;
		case 0x1D: KEYBD[2] &= ~KEY_Z; break;
		
        case 0x1E: KEYBD[1] &= ~KEY_1; break;
        case 0x1F: KEYBD[2] &= ~KEY_2; break;
        case 0x20: KEYBD[2] &= ~KEY_3; break;
        case 0x21: KEYBD[2] &= ~KEY_4; break;   
        case 0x22: KEYBD[2] &= ~KEY_5; break;
		case 0x23: KEYBD[2] &= ~KEY_6; break;	
		case 0x24: KEYBD[4] &= ~KEY_7; break;
		case 0x25: KEYBD[4] &= ~KEY_8; break;		

        case 0x26: KEYBD[4] &= ~KEY_9; break;
        case 0x27: KEYBD[4] &= ~KEY_0; break;

        case 0x28: KEYBD[4] &= ~KEY_RETURN; break;
        case 0x29: KEYBD[2] &= ~KEY_CNTRL; break;        
		case 0x2C: KEYBD[3] &= ~KEY_SPACE; break;
        case 0x2D: KEYBD[4] &= ~KEY_COLON; break;
        case 0x2E: KEYBD[4] &= ~KEY_EQUALS; break;
		case 0x33: KEYBD[4] &= ~KEY_SEMICOLON; break;
        case 0x36: KEYBD[4] &= ~KEY_COMMA; break;	
        case 0x37: KEYBD[4] &= ~KEY_DOT; break;
        case 0x38: KEYBD[4] &= ~KEY_SLASH; break;
        case 0x39: KEYBD[1] &= ~KEY_SHIFT; break;
		
		case 0x4F: KEYBD[3] &= ~KEY_RIGHT; break;
		case 0x50: KEYBD[2] &= ~KEY_LEFT; break;
		case 0x51: KEYBD[1] &= ~DOWN_BUTTON; break;
		case 0x52: KEYBD[1] &= ~UP_BUTTON; break;		
  
        default:break;
    }
}