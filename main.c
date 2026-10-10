#include <stdio.h>
#include "pico/stdlib.h"
#include <string.h>

#include "pico/multicore.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
// #include "hardware/i2c.h"

#include "ff.h"
#include "f_util.h"

#include "hardware/vreg.h"
#include <hardware/structs/qmi.h>
#include <hardware/structs/xip.h>

#include "UM6502pinout.h"
#include "VDP_int.pio.h"
#include "M6821.h"


// /VDP_RD 
#define VDP_RD      36
#define VDP_RD_MASK (1ULL << VDP_RD)

// /VDP_WR 
#define VDP_WR      37
#define VDP_WR_MASK (1ULL << VDP_WR)

#define PIN_6502_CLOCK  29

// RESET
#define PIN_6502_RESET  44

// CLK 2 MHz
#define CLK_2_MHz       28

// /AUD_W
// #define AUD_W       39
// #define AUD_W_MASK (1ULL << AUD_W)

// NES-compatible gamepad (shift register) pins
#define PIN_NES_JOYPAD_CLOCK 32
#define PIN_NES_JOYPAD_LATCH 33
#define PIN_NES_JOYPAD_DATA1  34
#define PIN_NES_JOYPAD_DATA2  35
#define NES_JOYPAD_SHIFT_COUNT 8

#define VDP_INT_PIO     pio0
#define VDP_INT_SM      0

uint16_t romsize;
uint16_t biossize;

static uint8_t __aligned(4) RAM[0x400];
static uint8_t __aligned(4) ROM1_2[0x8000];
static uint8_t __aligned(4) ROM0[0x4000];

volatile uint8_t joypad_read = 0;

void inInit(uint gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_pull_up(gpio);
}

void outInit(uint gpio,bool set_up_out) {
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, set_up_out);
}

static void initialize_pwm_pin(uint pin_number) {
    gpio_set_function(pin_number, GPIO_FUNC_PWM);
    const uint pwm_slice = pwm_gpio_to_slice_num(pin_number);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, clock_get_hz(clk_sys) / (4.0 * 2000000));
    pwm_config_set_wrap(&cfg, 3); // MAX PWM value
    pwm_init(pwm_slice, &cfg, true);
    pwm_set_gpio_level(pin_number, 2);
}

void setup_6502_pins() {
    // 6502 bus control inputs from CPU
    inInit(R_W_PIN); // Read/Write#
    // initialize_input_pin(F2_PIN);   // Phase signal (used to time bus cycles)

    // 6502 clock output (we drive the CPU clock)
    outInit(PIN_6502_CLOCK, 0);
    outInit(PIN_6502_RESET, 0);
    // 6502 control outputs (held high/inactive by default)
    // outInit(NMI6502_PIN, 1);
    // outInit(IRQ6502_PIN, 1);
    // outInit(RDY6502_PIN, 1);
    initialize_pwm_pin(CLK_2_MHz);

    outInit(AUD_W,1);
    outInit(VDP_RD,1);
    outInit(VDP_WR,1);
    // NES-style gamepad shift register lines
    inInit(PIN_NES_JOYPAD_DATA1);
    inInit(PIN_NES_JOYPAD_DATA2);
    outInit(PIN_NES_JOYPAD_CLOCK, 1);
    outInit(PIN_NES_JOYPAD_LATCH, 1);
}

// selector VDP_INT to NMI for coleco mode
void VDP_INT_init() {
    PIO piopio = VDP_INT_PIO;
    uint smsm  = VDP_INT_SM;
    pio_set_gpio_base(piopio, 16);
    const uint pio_offset = pio_add_program(piopio, &vdpintz80_program);
    vdpintz80_program_init(piopio, smsm, pio_offset);
    pio_sm_set_enabled(piopio, smsm, true);
}

// Read NES-compatible gamepads via latch/clocked serial interface.
// Returns an active-high button mask after converting from active-low bits.
uint16_t read_joystick_data() {
    uint8_t serial_data1 = 0;
    uint8_t serial_data2 = 0;
    uint16_t out_data;

    // Latch current button states into the controller's shift register
    gpio_put(PIN_NES_JOYPAD_LATCH, 1);
    busy_wait_us(20);
    gpio_put(PIN_NES_JOYPAD_LATCH, 0);
    busy_wait_us(20);

    // Shift in 8 bits (standard NES controller protocol)
    for (int i = 0; i < NES_JOYPAD_SHIFT_COUNT; i++) {
        gpio_put(PIN_NES_JOYPAD_CLOCK, 0);
        busy_wait_us(20);
        serial_data1 <<= 1;
        serial_data1 |= gpio_get(PIN_NES_JOYPAD_DATA1);
        serial_data2 <<= 1;
        serial_data2 |= gpio_get(PIN_NES_JOYPAD_DATA2);       
        gpio_put(PIN_NES_JOYPAD_CLOCK, 1);
        busy_wait_us(20);
    }
    out_data = (serial_data2<<8)|serial_data1;
    return out_data;
};

// 6502 write-side 
__always_inline static inline void write_6502_memory(const uint16_t address, const uint8_t value) {

    if (address < 0x0400) {RAM[address & 0x03ff] = value; return;}

    if ((address & 0xF000) == 0x1000){
// printf("write into PIA - address = %04X data = %02X \n",address,value) ;               
        PIA_Write(address,value);}
    
}

// 6502 read-side 
__always_inline static inline uint8_t read_6502_memory(const uint16_t address) {

    if (address < 0x0400) {return RAM[address & 0x03ff];}

    if ((address & 0xF000) == 0x2000) { gpio_put(VDP_RD,false);}

    if ((address & 0xF000) == 0x1000){
        joypad_read = 1;                    // start read joypad
        return PIA_Read(address);}
    //BIOS
    if (address >= 0xC000){
            if(biossize == 0x0800){return ROM0[address & 0x07ff];}
             else if(biossize == 0x4000){return ROM0[address & 0x3fff];}       
    }

    // if (address >= 0xC000) {return ROM0[address & 0x07ff];}
    if (address >= 0xC000) {return ROM0[address & 0x3fff];}

    //ROM
        switch (romsize & 0x7800)
        {
        case 0x1000:    
            // ROM = 4k
            if ((address >= 0xB000)&&(address < 0xC000))  {return ROM1_2[address & 0x0fff];}           
            break;            
        case 0x1800:    
            // ROM = 6k
            if ((address >= 0xB000)&&(address < 0xC000))  {return ROM1_2[address & 0x0fff];}           
            if ((address >= 0xA800)&&(address < 0xB000))   {return ROM1_2[(address & 0x07ff)+ 0x1000];}
            break;
        case 0x2000:    
            // ROM = 8k 
            if ((address >= 0xA000)&&(address < 0xC000))   {return ROM1_2[(address & 0x1fff)];}                      
            break;
        case 0x2800:    
            // ROM = 10k          
            if ((address >= 0xA000)&&(address < 0xC000))   {return ROM1_2[(address & 0x1fff)];} 
            if ((address >= 0x4000)&&(address < 0x4800))   {return ROM1_2[(address & 0x07ff)+ 0x2000];} 
            if ((address >= 0x7000)&&(address < 0x7800))   {return ROM1_2[(address & 0x07ff)+ 0x2000];}            
            break;
        case 0x3000:    
            // ROM = 12k
            if ((address >= 0xA000)&&(address < 0xC000))   {return ROM1_2[(address & 0x1fff)];} 
            if ((address >= 0x4000)&&(address < 0x5000))   {return ROM1_2[(address & 0x0fff)+ 0x2000];} 
            if ((address >= 0x7000)&&(address < 0x8000))   {return ROM1_2[(address & 0x0fff)+ 0x2000];} 
            break;
        case 0x4000:    
            // ROM = 16k
            if ((address >= 0xA000)&&(address < 0xC000))   {return ROM1_2[(address & 0x1fff)];}
            if ((address >= 0x8000)&&(address < 0xA000))   {return ROM1_2[(address & 0x1fff)+ 0x2000];} 
            break;
        case 0x4800:    
            // ROM = 18k
            if ((address >= 0xA000)&&(address < 0xC000))   {return ROM1_2[(address & 0x1fff)];}
            if ((address >= 0x8000)&&(address < 0xA000))   {return ROM1_2[(address & 0x1fff)+ 0x2000];}
            if ((address >= 0x7800)&&(address < 0x8000))   {return ROM1_2[(address & 0x07ff)+ 0x4000];} 
            break;                                                
        default:
            break;
        }
         
    return 0xFF;
}

// Time-critical 6502 bus service loop.
void __time_critical_func(handle_6502_bus)() {
    uint16_t reset_delay_counter = 0;
    uint16_t address; // latched 16-bit address from CPU
    extern unsigned long long int total_cycles;  // for PIA

    // Small programmable delay in NOPs to meet bus timing between edges
#define BUS_CYCLE_DELAY_NOPs 15         // 15 = 114Hz IRQ, 14 = 116Hz, 13 = 122Hz

    while (true) {

        // Phase 1: Clock high, CPU drives address and R/W#
        gpio_put(PIN_6502_CLOCK, 0);

        gpio_put(VDP_RD,true);
        gpio_put(VDP_WR,true);
        gpio_put(AUD_W,true);        

        gpio_set_dir_in_masked(DATA6502_MASK); // float data bus (input)

            for (uint8_t i = 0; i < BUS_CYCLE_DELAY_NOPs; i++) {asm volatile("nop \n nop \n nop \n nop");}

        // Sample address bus and R/W#; map address bits from GPIOs to 16-bit address
        address = (uint16_t)((gpio_get_all() & ADDR6502_MASK));
        // If read cycle, drive data onto bus
        if (gpio_get_all64() & R_W_MASK) {
                gpio_put_masked(DATA6502_MASK, read_6502_memory(address) << 16);          
            }else{
                if ((address & 0xF000) == 0x3000) {
                    gpio_put(VDP_WR,0);        
                }             
            }

        // Phase 2: Clock low, CPU may sample data; on writes we capture it
        gpio_put(PIN_6502_CLOCK, 1);

        for (uint8_t i = 0; i < BUS_CYCLE_DELAY_NOPs; i++) { asm volatile("nop \n nop \n nop \n nop ");}

        if (gpio_get_all64() & R_W_MASK) {
            // For read cycles, drive data bus outputs
            gpio_set_dir_out_masked(DATA6502_MASK);
        } else {
            // For write cycles, sample data from bus and decode write
            uint8_t value = (uint8_t) ((gpio_get_all64() & DATA6502_MASK)>> 16);
            write_6502_memory(address, value);           
        }
        for (uint8_t i = 0; i < BUS_CYCLE_DELAY_NOPs; i++) { asm volatile("nop \n nop \n nop "); }        

        total_cycles++; // for PIA 

        // Apply a short reset pulse only on startup


        if (reset_delay_counter < 8) {
            reset_delay_counter++;
            gpio_put(PIN_6502_RESET, 0);
        } else {
            gpio_put(PIN_6502_RESET, 1);
        }

 
    }
}

FRESULT load_rom_from_sd(const char *path, uint8_t *buf, size_t buf_max, size_t *out_size) {
    FIL file_sd;
    UINT br = 0;
    FILINFO fno;

    // 1. Проверяем существование и размер
    FRESULT res = f_stat(path, &fno);
    if (res != FR_OK) {
        printf("[SD] File stat failed: %s (code %d)\n", path, res);
        return res;
    }

    size_t fsize = fno.fsize;
    printf("[SD] ROM size: %lu bytes\n", (unsigned long)fsize);

    // 2. Проверка на пустой или слишком большой файл
    if (fsize == 0 || fsize > buf_max) {
        printf("[SD] Invalid size: %lu, max allowed: %lu\n",
               (unsigned long)fsize, (unsigned long)buf_max);
        return FR_INVALID_PARAMETER;
    }

    // 3. Открываем и читаем
    res = f_open(&file_sd, path, FA_READ);
    if (res != FR_OK) {
        printf("[SD] Cannot open file: %s (code %d)\n", path, res);
        return res;
    }

    res = f_read(&file_sd, buf, fsize, &br);
    f_close(&file_sd);

    // 4. Главная проверка: сколько реально прочиталось
    if (res == FR_OK && br == fsize) {
        *out_size = fsize;
        printf("[SD] Success: read %lu bytes\n", (unsigned long)br);
        return FR_OK;
    } else {
        // Вот тут ты получаешь честные цифры: что хотел и что получил
        printf("[SD] Read mismatch: requested %lu, got %lu (code %d)\n",
               (unsigned long)fsize, (unsigned long)br, res);
        return res;  // возвращаем настоящий код ошибки FatFs
    }
}



int main()
{
    vreg_disable_voltage_limit();
    vreg_set_voltage(VREG_VOLTAGE_1_60);
    qmi_hw->m[0].timing = 0x60007304; // 4x FLASH divisor
    sleep_ms(100);
    set_sys_clock_hz(315 * MHZ, 1); // fallback to failsafe clocks

    stdio_init_all();

    // Initialize 6502 address/data bus directions
    gpio_init_mask(ADDR6502_MASK | DATA6502_MASK);
    gpio_set_dir_in_masked(ADDR6502_MASK | DATA6502_MASK);

    // Configure all remaining 6502 and controller pins
    setup_6502_pins();

        // Initialize select out interrupt from VDP to 6502

    VDP_INT_init();

sleep_ms(5000);

// 1. Монтируем SD
FATFS fs;
if (FR_OK != f_mount(&fs, "", 1)) {
    printf("SD Card not mounted!\n");
    while (1) {}  // вешаем плату, чтобы видеть ошибку
}

// Загрузка BIOS
size_t bios_size = 0;
FRESULT res = load_rom_from_sd("/CRV/BIOS/main.rom", ROM0, sizeof(ROM0), &bios_size);

if (res != FR_OK) {
    printf("[CRV] Failed to load BIOS: code %d\n", res);
} else {
    printf("[CRV] BIOS loaded: %lu bytes\n", (unsigned long)bios_size);
}

// Загрузка картриджа
size_t cart_size = 0;
res = load_rom_from_sd("/CRV/CART/game.rom", ROM1_2, sizeof(ROM1_2), &cart_size);

if (res != FR_OK) {
    printf("[CRV] Failed to load cartridge: code %d\n", res);
} else {
    printf("[CRV] Cartridge loaded: %lu bytes\n", (unsigned long)cart_size);
}

// CLEAR RAM
    memset(RAM, 0x00, 0x400);
// START
    multicore_launch_core1(handle_6502_bus);

    while (true) {

        // for (size_t i = 1; i < 16; i++)
        // {
        //     for (size_t y = 0; y < 16; y++)
        //     {
        //         printf(" %02X ", RAM[(i+16*y)+256]);
        //     }
        //     printf("\n");         
        // }
        // printf("\n");         
        // busy_wait_ms(1000);

        if(joypad_read == 1 ){
            busy_wait_ms(4);            
            convert_nes_to_CV_joypad(read_joystick_data());
            busy_wait_ms(4);
            joypad_read = 0;
        }
        

    } // while    
    
}//main
