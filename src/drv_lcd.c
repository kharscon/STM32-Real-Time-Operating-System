#include "main.h"
#include "drv_lcd.h"
#include "drv_lcd_font.h"
#include <string.h>

/* * SECTION 1: MEMORY MAPPING FOR PF0 (A0)
 * Base Address: 0x60000000 (Bank 1)
 * RS Pin: PF0 (FSMC_A0)
 * * Logic:
 * - Address 0x60000000 -> A0 is 0 -> LCD Command
 * - Address 0x60000001 -> A0 is 1 -> LCD Data
 * (Note: For 8-bit width, address shifts by 0, so bit 0 maps to A0)
 */
/* BANK 3 (NE3) Address = 0x68000000 */
#define LCD_BASE        ((uint32_t)0x68000000)
#define LCD_REG         (*((volatile uint8_t *)LCD_BASE))
#define LCD_RAM         (*((volatile uint8_t *)(LCD_BASE + 0x40000))) // A18 Offset

/* REMOVE any "LCD_CS_LOW()" calls from your functions.
   Let the hardware handle CS automatically. */
_lcd_dev lcddev;
uint16_t BACK_COLOR = WHITE;
uint16_t FORE_COLOR = BLACK;

/* SECTION 2: GPIO CHIP SELECT CONTROL */
// Since CS is on PF13 (not a hardware NE pin), we control it manually.
// Active Low: 0 = Selected, 1 = Deselected
void LCD_CS_LOW(void)  { HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET); }
void LCD_CS_HIGH(void) { HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET); }

/* SECTION 3: BUS WRITES */
void LCD_WR_REG(uint8_t reg) {
    LCD_CS_LOW();  // Select LCD
    LCD_REG = reg; // Write Command
    LCD_CS_HIGH(); // Deselect
}

void LCD_WR_DATA8(uint8_t data) {
    LCD_CS_LOW();
    LCD_RAM = data; // Write Data
    LCD_CS_HIGH();
}

void LCD_WR_DATA16(uint16_t data) {
    LCD_CS_LOW();
    LCD_RAM = (data >> 8);   // High Byte
    LCD_RAM = (data & 0xFF); // Low Byte
    LCD_CS_HIGH();
}

void LCD_WriteRAM_Prepare(void) {
    LCD_WR_REG(lcddev.wramcmd);
}

void LCD_WriteRAM(uint16_t color) {
    LCD_WR_DATA16(color);
}

/* SECTION 4: INITIALIZATION */
void LCD_Reset(void) {
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(50);
}

void LCD_Init(void) {
    // Ensure CS is high (inactive) initially
    LCD_CS_HIGH();

    LCD_Reset();
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET); // Backlight On

    // 1. Sleep Out
    LCD_WR_REG(0x11);
    HAL_Delay(120);

    // 2. Pixel Format (RGB565)
    LCD_WR_REG(0x36); LCD_WR_DATA8(0x00);
    LCD_WR_REG(0x3A); LCD_WR_DATA8(0x05);

    // 3. Power & Gamma (Standard ST7789)
    LCD_WR_REG(0xB2); LCD_WR_DATA8(0x0C); LCD_WR_DATA8(0x0C); LCD_WR_DATA8(0x00); LCD_WR_DATA8(0x33); LCD_WR_DATA8(0x33);
    LCD_WR_REG(0xB7); LCD_WR_DATA8(0x35);
    LCD_WR_REG(0xBB); LCD_WR_DATA8(0x19);
    LCD_WR_REG(0xC0); LCD_WR_DATA8(0x2C);
    LCD_WR_REG(0xC2); LCD_WR_DATA8(0x01);
    LCD_WR_REG(0xC3); LCD_WR_DATA8(0x12);
    LCD_WR_REG(0xC4); LCD_WR_DATA8(0x20);
    LCD_WR_REG(0xC6); LCD_WR_DATA8(0x0F);
    LCD_WR_REG(0xD0); LCD_WR_DATA8(0xA4); LCD_WR_DATA8(0xA1);

    LCD_WR_REG(0xE0); LCD_WR_DATA8(0xD0); LCD_WR_DATA8(0x04); LCD_WR_DATA8(0x0D); LCD_WR_DATA8(0x11); LCD_WR_DATA8(0x13); LCD_WR_DATA8(0x2B); LCD_WR_DATA8(0x3F); LCD_WR_DATA8(0x54); LCD_WR_DATA8(0x4C); LCD_WR_DATA8(0x18); LCD_WR_DATA8(0x0D); LCD_WR_DATA8(0x0B); LCD_WR_DATA8(0x1F); LCD_WR_DATA8(0x23);
    LCD_WR_REG(0xE1); LCD_WR_DATA8(0xD0); LCD_WR_DATA8(0x04); LCD_WR_DATA8(0x0C); LCD_WR_DATA8(0x11); LCD_WR_DATA8(0x13); LCD_WR_DATA8(0x2C); LCD_WR_DATA8(0x3F); LCD_WR_DATA8(0x44); LCD_WR_DATA8(0x51); LCD_WR_DATA8(0x2F); LCD_WR_DATA8(0x1F); LCD_WR_DATA8(0x1F); LCD_WR_DATA8(0x20); LCD_WR_DATA8(0x23);

    // 4. Display On
    LCD_WR_REG(0x21); // Inversion On
    LCD_WR_REG(0x29); // Display On

    // 5. Parameters
    lcddev.width = 240;
    lcddev.height = 240;
    lcddev.setxcmd = 0x2A;
    lcddev.setycmd = 0x2B;
    lcddev.wramcmd = 0x2C;

    LCD_Clear(WHITE);
}

// (Keep LCD_SetWindow, LCD_Clear, LCD_DrawPixel, LCD_ShowChar, LCD_ShowString exactly as they were)
/* ... PASTE SECTION 4 and 5 FROM YOUR PREVIOUS FILE HERE ... */
void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    LCD_WR_REG(lcddev.setxcmd);
    LCD_WR_DATA8(x1 >> 8); LCD_WR_DATA8(x1 & 0xFF);
    LCD_WR_DATA8(x2 >> 8); LCD_WR_DATA8(x2 & 0xFF);
    LCD_WR_REG(lcddev.setycmd);
    LCD_WR_DATA8(y1 >> 8); LCD_WR_DATA8(y1 & 0xFF);
    LCD_WR_DATA8(y2 >> 8); LCD_WR_DATA8(y2 & 0xFF);
    LCD_WriteRAM_Prepare();
}

void LCD_Clear(uint16_t color) {
    LCD_SetWindow(0, 0, lcddev.width-1, lcddev.height-1);
    uint32_t total = lcddev.width * lcddev.height;
    // Note: We don't toggle CS for every pixel to speed it up
    LCD_CS_LOW();
    for (uint32_t i = 0; i < total; i++) {
        LCD_RAM = (color >> 8);
        LCD_RAM = (color & 0xFF);
    }
    LCD_CS_HIGH();
}

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= lcddev.width || y >= lcddev.height) return;
    LCD_SetWindow(x, y, x, y);
    LCD_WR_DATA16(color);
}

void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t color, uint16_t back_color) {
    uint8_t temp, t;
    uint16_t pos;
    if (x > lcddev.width - 8 || y > lcddev.height - 16) return;
    num = num - ' ';
    LCD_SetWindow(x, y, x + 7, y + 15);

    LCD_CS_LOW(); // Optimization: Hold CS low for the whole character
    for (pos = 0; pos < 16; pos++) {
        temp = asc2_1608[num * 16 + pos];
        for (t = 0; t < 8; t++) {
            if (temp & 0x80) { LCD_RAM=(color>>8); LCD_RAM=(color&0xFF); }
            else             { LCD_RAM=(back_color>>8); LCD_RAM=(back_color&0xFF); }
            temp <<= 1;
        }
    }
    LCD_CS_HIGH();
}

void LCD_ShowString(uint16_t x, uint16_t y, char *p, uint16_t color, uint16_t back_color) {
    while (*p != '\0') {
        if (x > lcddev.width - 8) { x = 0; y += 16; }
        if (y > lcddev.height - 16) break;
        LCD_ShowChar(x, y, *p, color, back_color);
        x += 8; p++;
    }
}
