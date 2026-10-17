

// Address Bus (A0 - A16)
#define ADDR6502_MASK  (0x0000FFFF) // Bits 0-16

// Data Bus (D0 - D7)
#define DATA6502_MASK  (0x000000FF << 16)

// R/W
#define R_W_PIN      24
#define R_W_MASK   (1u << R_W_PIN)

// SYNC
#define SYNC_PIN      25
#define SYNC_MASK  (1u << SYNC_PIN)

// F1
#define F1_PIN    26
#define F1_MASK   (1ULL << F1_PIN)

// F2
#define F2_PIN    27
#define F2_MASK   (1ULL << F2_PIN)

// SetOvFlag
// #define SetOvFlag_PIN      34
// #define SetOvFlag_MASK (1u << M1_PIN)

// RDY
// #define RDY6502_PIN    35
// #define RDY_MASK (1u << WAIT_PIN)

// /IRQ
// #define IRQ6502_PIN     36
// #define IRQ6502_MASK (1u << IRQ6502_PIN)

// /NMI
// #define NMI6502_PIN     37
// #define NMI_MASK (1u << NMI_PIN)