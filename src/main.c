// ES32D26 hardware test (ESP-IDF):
// - Reads digital inputs via 74HC165 and prints them.
// - Shifts relay states out to 74HC595 and exercises relays step-by-step.
// - Prints relay GPIO levels for diagnostics.
//
// Relay test sequence diagram (2 s between steps):
//   Step 0: R0 -> R7 ON  (mask grows: 0x01, 0x03, ..., 0xFF)
//   Step 1: R7 -> R0 OFF (mask shrinks: 0x7F, 0x3F, ..., 0x00)
//   Step 2: ALL ON       (mask: 0xFF)
//   Step 3: ALL OFF      (mask: 0x00)
//   Step 4: STOP         (mask: 0x00)
// Relay bit legend: bit0=R1, bit1=R2, ..., bit7=R8
// Input bit legend: bit0=IN1, bit1=IN2, ..., bit7=IN8
// Input logic note: printed bits are inverted (1 = LED ON, 0 = LED OFF).
// Shift-register GPIO map:
//   74HC595 (relays)  DATA=GPIO12, CLOCK=GPIO22, LATCH=GPIO23, OE=GPIO13
//   74HC165 (inputs)  DATA=GPIO15, CLOCK=GPIO2,  LOAD=GPIO0
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "esp_log.h"
#include <esp_rom_sys.h>
#include "esp_rom_gpio.h"

// Pin definitions for 74HC595 (Relays)
#define RELAY_DATA 12
#define RELAY_CLOCK 22
#define RELAY_LATCH 23
#define RELAY_OE 13
#define RELAY_OE_ACTIVE_LOW 1 // 74HC595 OE is active-low on this board

// Pin definitions for 74HC165 (Digital Inputs)
#define INPUT_DATA 15
#define INPUT_CLOCK 2
#define INPUT_LOAD 0

// Analog Inputs (index -> GPIO): AI0=34, AI1=39, AI2=35, AI3=36
const int AI_PINS[] = {34, 39, 35, 36};
// DAC Outputs (index -> GPIO): DAC0=25, DAC1=26 (not used in this test)
const int DAC_PINS[] = {25, 26};

// Global state for the relay test sequence.
uint8_t relayState = 0;
int step = 0;
int relayIndex = 0;
// Tag used by ESP_LOG for consistent log prefixing.
static const char *TAG = "HWTEST";

void updateRelays(uint8_t mask) {
    // Enable the 74HC595 outputs (OE is active-low on this board).
    gpio_set_level(RELAY_OE, RELAY_OE_ACTIVE_LOW ? 0 : 1); // Enable outputs
    // Begin shifting out 8 bits into the 74HC595.
    gpio_set_level(RELAY_LATCH, 0);
    for (int i = 7; i >= 0; i--) {
        // Clock each bit into the shift register.
        gpio_set_level(RELAY_CLOCK, 0);
        gpio_set_level(RELAY_DATA, (mask >> i) & 0x01);
        gpio_set_level(RELAY_CLOCK, 1);
    }
    // Latch the shifted data to the outputs.
    gpio_set_level(RELAY_LATCH, 1);
    esp_rom_delay_us(5); // krátky delay po LATCH HIGH
    // Debug: read back the physical pin levels after latching.
    ESP_LOGI(TAG, "[RELAY] OE=%d LATCH=%d DATA=%d CLOCK=%d",
        gpio_get_level(RELAY_OE),
        gpio_get_level(RELAY_LATCH),
        gpio_get_level(RELAY_DATA),
        gpio_get_level(RELAY_CLOCK));
    // Store the current relay mask for the next step.
    relayState = mask;
}

// Read 8 bits from 74HC165, MSB first (Arduino-style logic).
uint8_t shiftIn(uint8_t dataPin, uint8_t clockPin) {
    uint8_t value = 0;
    for (int i = 7; i >= 0; i--) {
        // Pull clock low, read the bit, then clock high.
        gpio_set_level(clockPin, 0); // CLOCK LOW pred čítaním
        esp_rom_delay_us(2);
        int bit = gpio_get_level(dataPin);
        value |= (bit << i);
        gpio_set_level(clockPin, 1); // CLOCK HIGH po čítaní
        esp_rom_delay_us(2);
    }
    return value;
}

uint8_t readInputs() {
    // Load the current input states into the 74HC165 shift register.
    gpio_set_level(INPUT_LOAD, 0);
    esp_rom_delay_us(5);
    gpio_set_level(INPUT_LOAD, 1);
    // Shift the 8-bit value out.
    uint8_t inputs = shiftIn(INPUT_DATA, INPUT_CLOCK);
    return inputs;
}

void printStatus() {
    // Read digital inputs and one analog input for status output.
    uint8_t inputs = readInputs();
    int ai34 = 0;
    ai34 = adc1_get_raw(ADC1_CHANNEL_6);
    // Input legend: bit0=IN1, bit1=IN2, ..., bit7=IN8
    printf("Inputs: ");
    // Invertované bity: 1 = LED svieti, 0 = LED nesvieti (rovnako ako Arduino)
    // Tj. v logu sa zobrazí 1, keď je vstup aktívny.
    for (int i = 0; i < 8; i++) {
        char bit = ((~inputs) & (1 << i)) ? '1' : '0';
        putchar(bit);
    }
    printf(" | AI34: %d\n", ai34);
    for (int i = 0; i < 8; i++) {
        // Per-input line helps see exactly which input changes.
        printf("[TEST] IN%d (bit %d): %d\n", i+1, i, ((~inputs) & (1 << i)) ? 1 : 0);
    }
}

void app_main() {
    // Hardware initialization first.
    // Ensure GPIO function on pins that may default to JTAG/strapping roles.
    esp_rom_gpio_pad_select_gpio(RELAY_DATA);
    esp_rom_gpio_pad_select_gpio(RELAY_CLOCK);
    esp_rom_gpio_pad_select_gpio(RELAY_LATCH);
    esp_rom_gpio_pad_select_gpio(RELAY_OE);
    // Input-output mode keeps readback working for diagnostics.
    gpio_set_direction(RELAY_DATA, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(RELAY_CLOCK, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(RELAY_LATCH, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(RELAY_OE, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(RELAY_OE, RELAY_OE_ACTIVE_LOW ? 0 : 1); // Enable outputs
    // Start with all relays off.
    updateRelays(0); // All off
    // Configure shift-register inputs.
    gpio_set_direction(INPUT_DATA, GPIO_MODE_INPUT);
    gpio_set_direction(INPUT_CLOCK, GPIO_MODE_OUTPUT);
    gpio_set_direction(INPUT_LOAD, GPIO_MODE_OUTPUT);
    gpio_set_level(INPUT_LOAD, 1);
    // Configure analog and DAC pins (DAC not used in this test).
    for (int i = 0; i < 4; i++) gpio_set_direction(AI_PINS[i], GPIO_MODE_INPUT);
    for (int i = 0; i < 2; i++) gpio_set_direction(DAC_PINS[i], GPIO_MODE_OUTPUT);
    // Configure ADC once at startup.
    // Note: ADC1_CHANNEL_6 maps to GPIO34 on ESP32.
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); // Deprecated, but used for compatibility
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "ES32D26 Hardware Test Start");
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        // Stop after the full relay test sequence completes.
        if (step > 5) {
            ESP_LOGI(TAG, "Program halted. All relays OFF.");
            while (1) vTaskDelay(pdMS_TO_TICKS(1000));
        }
        // Print current input and analog status.
        printStatus();
        switch (step) {
            case 0:
                // Turn relays on one by one (0 -> 7).
                ESP_LOGI(TAG, "Step 0: Turning on relays one by one. Index: %d", relayIndex);
                relayState |= (1 << relayIndex);
                updateRelays(relayState);
                relayIndex++;
                if (relayIndex >= 8) {
                    step++;
                    relayIndex = 7;
                }
                break;
            case 1:
                // Turn relays off one by one (7 -> 0).
                ESP_LOGI(TAG, "Step 1: Turning off relays one by one. Index: %d", relayIndex);
                relayState &= ~(1 << relayIndex);
                updateRelays(relayState);
                relayIndex--;
                if (relayIndex < 0) {
                    step++;
                }
                break;
            case 2:
                // Turn all relays on at once.
                ESP_LOGI(TAG, "Step 2: Turning all relays ON");
                relayState = 0xFF;
                updateRelays(relayState);
                step++;
                break;
            case 3:
                // Turn all relays off at once.
                ESP_LOGI(TAG, "Step 3: Turning all relays OFF");
                relayState = 0x00;
                updateRelays(relayState);
                step++;
                break;
            case 4:
                // Final step: keep everything off and stop.
                ESP_LOGI(TAG, "Step 4: Test complete. All relays OFF. Program will stop.");
                relayState = 0x00;
                updateRelays(relayState);
                step++;
                break;
        }
        // Wait before the next step so changes are visible.
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}