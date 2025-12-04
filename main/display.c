#include "driver/spi_master.h"
#include "display.h"
#include <string.h>
#include <assert.h>
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_log.h"
static const char *TAG = "display";


// If above / below these, diplay very x
int v_low = 50;
int v_high = 250;

int g_high;
int g_low;

uint8_t g_trend;
uint8_t g_indicator;

// Arrows and ranges
static const uint8_t ARROW_DOUBLE_UP[8] = {
    0b00011000,
    0b00111100,
    0b01111110,
    0b00000000,
    0b00011000,
    0b00111100,
    0b01111110,
    0b00000000
};
static const uint8_t ARROW_DOUBLE_DOWN[8] = {
    0b01111110,
    0b00111100,
    0b00011000,
    0b00000000,
    0b01111110,
    0b00111100,
    0b00011000,
    0b00000000
};
static const uint8_t ARROW_DIAG_DOWN[8] = {
    0b00000000,
    0b01000000,
    0b01100000,
    0b00110010,
    0b00011110,
    0b00001110,
    0b00011110,
    0b00000000
};
static const uint8_t ARROW_DIAG_UP[8] = {
    0b00000000,
    0b00011110,
    0b00001110,
    0b00011110,
    0b00110010,
    0b01100000,
    0b01000000,
    0b00000000
};
static const uint8_t ARROW_FLAT[8] = {
    0b00000000,
    0b00001000,
    0b00001100,
    0b01111110,
    0b01111110,
    0b00001100,
    0b00001000,
    0b00000000

};
static const uint8_t ARROW_DOWN[8] = {
    0b00000000,
    0b00011000,
    0b00011000,
    0b00011000,
    0b01111110,
    0b00111100,
    0b00011000,
    0b00000000
};
static const uint8_t ARROW_UP[8] = {
    0b00000000,
    0b00011000,
    0b00111100,
    0b01111110,
    0b00011000,
    0b00011000,
    0b00011000,
    0b00000000
};
static const uint8_t GOOD[8] = {
    0b00000000,
    0b01100110,
    0b01100110,
    0b00000000,
    0b00000000,
    0b01000010,
    0b00111100,
    0b00000000
};
static const uint8_t HIGH[8] = {
    0b00000000,
    0b01100110,
    0b01100110,
    0b01111110,
    0b01111110,
    0b01100110,
    0b01100110,
    0b00000000
};
static const uint8_t LOW[8] = {
    0b00000000,
    0b00110000,
    0b00110000,
    0b00110000,
    0b00110000,
    0b00111100,
    0b00111100,
    0b00000000
};
static const uint8_t VHIGH[8] = {
    0b00000000,
    0b01010000,
    0b01010000,
    0b00100000,
    0b00001010,
    0b00001110,
    0b00001010,
    0b00000000
};
static const uint8_t VLOW[8] = {
    0b00000000,
    0b01010000,
    0b01010000,
    0b00100000,
    0b00001000,
    0b00001000,
    0b00001100,
    0b00000000
};
static const uint8_t DASH[8] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b01111110,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
};
static const uint8_t CLEAR[8] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000
};


spi_device_handle_t max7219_handle;
uint8_t framebuffer[8] = {0};


void max7219_send_cmd(uint16_t cmd)
{
    // Build the 2-byte SPI buffer (1 chips × 16 bits)
    uint8_t buf[2];

    // Fill buffer
    buf[0] = (cmd >> 8) & 0xFF;
    buf[1] = cmd & 0xFF;

    spi_transaction_t t = {
        .length    = 8 * 2,
        .tx_buffer = buf,
    };

    // Blocking transmit — CS toggled automatically
    esp_err_t ret = spi_device_transmit(max7219_handle, &t);
    assert(ret == ESP_OK);
}

void max7219_clear(void) {
    max7219_send_cmd(MAX7219_CMD(MAX7219_REG_DIGIT0, 0x00)); 
    max7219_send_cmd(MAX7219_CMD(MAX7219_REG_DIGIT1, 0x00)); 
    max7219_send_cmd(MAX7219_CMD(MAX7219_REG_DIGIT2, 0x00)); 
    max7219_send_cmd(MAX7219_CMD(MAX7219_REG_DIGIT3, 0x00)); 
    max7219_send_cmd(MAX7219_CMD(MAX7219_REG_DIGIT4, 0x00)); 
    max7219_send_cmd(MAX7219_CMD(MAX7219_REG_DIGIT5, 0x00)); 
    max7219_send_cmd(MAX7219_CMD(MAX7219_REG_DIGIT6, 0x00)); 
    max7219_send_cmd(MAX7219_CMD(MAX7219_REG_DIGIT7, 0x00)); 
}

void max7219_init(void) {
    // Initialize all required settings for each max7219 in the chain of 4
    max7219_send_cmd(MAX7219_SHUTDOWN_INIT);   
    max7219_send_cmd(MAX7219_TEST_INIT);
    max7219_send_cmd(MAX7219_DECODE_INIT);
    max7219_send_cmd(MAX7219_SCAN_INIT);       
    max7219_send_cmd(MAX7219_INTENSITY_INIT);   

    memset(framebuffer, 0, sizeof(framebuffer));


    max7219_clear();
}

void write_glyph_to_framebuffer(const uint8_t* glyph) {
    for(int row = 0; row < 8; row ++) {
        framebuffer[row] = glyph[row];
    }
}

const uint8_t* get_glyph(int code) {
    switch (code) {

        case SYM_GOOD:          return GOOD;
        case SYM_HIGH:          return HIGH;
        case SYM_LOW:           return LOW;
        case SYM_VHIGH:         return VHIGH;
        case SYM_VLOW:          return VLOW;
        case SYM_ARROW_UP:      return ARROW_UP;
        case SYM_ARROW_DOWN:    return ARROW_DOWN;
        case SYM_ARROW_RIGHT:   return ARROW_FLAT;
        case SYM_ARROW_UR:      return ARROW_DIAG_UP;
        case SYM_ARROW_DR:      return ARROW_DIAG_DOWN;
        case SYM_DOUBLE_UP:     return ARROW_DOUBLE_UP;
        case SYM_DOUBLE_DOWN:   return ARROW_DOUBLE_DOWN;

        case SYM_DASH:        return DASH;
        case SYM_CLEAR:       return CLEAR;

        default:              return DASH;
    }
}


void update_framebuffer(int code0) {
    // Get all glyphs
    const uint8_t* glyph0 = get_glyph(code0);

    // Write each to framebuffer
    write_glyph_to_framebuffer(glyph0);

}

int convert_bg(int bg) {
    if (bg > g_high) {
        if(bg > v_high) {
            return SYM_VHIGH;
        }
        return SYM_HIGH;
    }
    else if(bg < g_low) {
        if(bg < v_low) {
            return SYM_VLOW;
        }
        return SYM_LOW;
    }
    else {
        return SYM_GOOD;
    }
}

int convert_trend(const char *trend) {
    if (!trend) {
        return SYM_DASH;
    }

    if (strcmp(trend, "Flat") == 0)
        return SYM_ARROW_RIGHT;

    if (strcmp(trend, "SingleUp") == 0)
        return SYM_ARROW_UP;

    if (strcmp(trend, "SingleDown") == 0)
        return SYM_ARROW_DOWN;

    if (strcmp(trend, "DoubleUp") == 0)
        return SYM_DOUBLE_UP;

    if (strcmp(trend, "DoubleDown") == 0)
        return SYM_DOUBLE_DOWN;

    if (strcmp(trend, "FortyFiveDown") == 0)
        return SYM_ARROW_DR;

    if (strcmp(trend, "FortyFiveUp") == 0)
        return SYM_ARROW_UR;

    return SYM_DASH;
}

void write_framebuffer_to_max7219() {
    uint8_t reg;
    uint16_t cmd;
    uint8_t data;

    for (int row = 0; row < 8; row ++) {
        reg = MAX7219_REG_DIGIT0 + row;
        data = framebuffer[row];
        cmd = MAX7219_CMD(reg, data);

        max7219_send_cmd(cmd);

    }
}

// Set all display matrices to -----
void set_loading_display(void) {
    // Update framebuffer with dash
    update_framebuffer(SYM_DASH);

    // Send framebuffer data to the display
    write_framebuffer_to_max7219();
}



// Update display values with  blood sugar displays
void update_display(int bg_level, const char *trend) {
    g_trend = convert_trend(trend);
    g_indicator = convert_bg(bg_level);

}   


void set_ranges(int low, int high) {
    g_low = low;
    g_high = high;

    ESP_LOGI(TAG, "Targets set: Low=%d, High=%d", low, high);

}

#define TOGGLE_INTERVAL_MS 3000

void display_task(void *pvParameter) {
    static int64_t last_toggle_time = 0;
    static bool is_showing_trend = false; 

    while (1) {
        int64_t current_time = esp_timer_get_time(); 

        if ((current_time - last_toggle_time) / 1000 >= TOGGLE_INTERVAL_MS) {
            is_showing_trend = !is_showing_trend; 
            last_toggle_time = current_time;     
        }

        if (is_showing_trend) {
            update_framebuffer(g_trend); 
        } else {
            update_framebuffer(g_indicator);
        }

        write_framebuffer_to_max7219();
        
        vTaskDelay(pdMS_TO_TICKS(50)); 
    }
}