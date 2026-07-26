/* ==========================================================================
   ESP32 FIRMWARE — FSR dance pad + I2S speaker + LEDs
   --------------------------------------------------------------------------
   Responsibilities of this board:
     1. Read 4 FSR pressure pads (Left/Down/Up/Right)
     2. When a pad is pressed past a threshold, send a hit event over
        USB serial as plain text, e.g.  "HIT:0\n"  (0=Left,1=Down,2=Up,3=Right)
     3. Play tones through the I2S amp + speaker on each hit
     4. Flash the corresponding LED on each hit for visual feedback

   The laptop program (in /laptop_visual) reads the "HIT:x" lines over
   serial, updates score/visuals, and shows the game on screen.

   Wiring:
     FSR Left   -> GPIO34 (ADC1_CH6)
     FSR Down   -> GPIO35 (ADC1_CH7)
     FSR Up     -> GPIO32 (ADC1_CH4)
     FSR Right  -> GPIO33 (ADC1_CH5)
     (each FSR wired as a voltage divider with a 70K resistor to GND)

     I2S Amp (MAX98357A):
     BCLK -> GPIO26
     LRC  -> GPIO25
     DOUT -> GPIO22

     LEDs (single colour, one per pad):
     LED Left  -> GPIO14 (via 330 ohm resistor)
     LED Down  -> GPIO13 (via 330 ohm resistor)
     LED Up    -> GPIO27 (via 330 ohm resistor)
     LED Right -> GPIO4  (via 330 ohm resistor)
   ========================================================================== */

#include <Arduino.h>
#include <math.h>
#include <driver/i2s.h>

/* ---------------------------- Config ------------------------------------ */

#define I2S_BCLK    26
#define I2S_LRC     25
#define I2S_DOUT    22
#define SAMPLE_RATE 44100
#define VOLUME      6000       /* int16 amplitude, keep well under 32767 */

#define NUM_LANES 4
const int FSR_PINS[NUM_LANES] = { 34, 35, 32, 33 }; /* L, D, U, R */

/* Raw ADC reads range 0-4095 on ESP32. Tune this after testing your
   actual FSRs -- press one firmly and watch the Serial output to find
   a good threshold. */
#define FSR_THRESHOLD 1200

/* Minimum time between accepted hits per lane, to avoid one press
   firing multiple times as pressure fluctuates ("debounce"). */
#define HIT_COOLDOWN_MS 150

/* LED pins — one per pad, same order as FSR_PINS: L, D, U, R
   Each LED wired with a 330 ohm current limiting resistor to GND. */
const int LED_PINS[NUM_LANES] = { 14, 13, 27, 4 }; /* L, D, U, R */

/* How long the LED stays on after a hit in milliseconds */
#define LED_FLASH_MS 150

/* ---------------------------- State --------------------------------------- */

unsigned long lastHitTime[NUM_LANES]  = {0, 0, 0, 0};
bool wasPressed[NUM_LANES]            = {false, false, false, false};

/* LED timer — stores millis() when each LED was turned on.
   0 means the LED is currently off and not timing. */
unsigned long ledOnTime[NUM_LANES]    = {0, 0, 0, 0};

/* ---------------------------- LED functions -------------------------------- */

/* ledInit
   Configures all LED pins as outputs and ensures they start off. */
void ledInit() {
    for (int i = 0; i < NUM_LANES; i++) {
        pinMode(LED_PINS[i], OUTPUT);
        digitalWrite(LED_PINS[i], LOW);
    }
}

/* ledHit
   Turns on the LED for the given lane and records the timestamp.
   ledUpdate() will turn it off after LED_FLASH_MS milliseconds. */
void ledHit(int lane) {
    digitalWrite(LED_PINS[lane], HIGH);
    ledOnTime[lane] = millis();
}

/* ledUpdate
   Checks all LED timers every loop and turns off any LEDs whose
   flash duration has expired. Must be called every loop iteration.
   Non-blocking — no delays used. */
void ledUpdate() {
    unsigned long now = millis();
    for (int i = 0; i < NUM_LANES; i++) {
        if (ledOnTime[i] > 0 && now - ledOnTime[i] >= LED_FLASH_MS) {
            digitalWrite(LED_PINS[i], LOW);
            ledOnTime[i] = 0;
        }
    }
}

/* ---------------------------- I2S audio ----------------------------------- */

void i2sInit() {
    i2s_config_t i2s_config = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = 0,
        .dma_buf_count        = 8,
        .dma_buf_len          = 64,
        .use_apll             = false
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num   = I2S_BCLK,
        .ws_io_num    = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("i2s_driver_install failed: %d\n", err);
    }
    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("i2s_set_pin failed: %d\n", err);
    }
}

/* Non-blocking-ish tone player: writes a short burst of samples for a
   hit sound when a pad is pressed. Kept short so it does not stall
   FSR polling for long. For a full background music track, consider
   moving audio to a separate FreeRTOS task on core 0. */
void playTone(float freq, int duration_ms) {
    int totalSamples = (SAMPLE_RATE * duration_ms) / 1000;
    int samplesWritten = 0;
    float phase = 0;

    while (samplesWritten < totalSamples) {
        int16_t buffer[64];
        for (int i = 0; i < 64; i++) {
            buffer[i] = (int16_t)(VOLUME * sinf(phase));
            phase += 2.0f * PI * freq / SAMPLE_RATE;
            if (phase > 2.0f * PI) phase -= 2.0f * PI;
        }
        size_t bytes_written;
        i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &bytes_written, portMAX_DELAY);
        samplesWritten += 64;
    }
}

/* Distinct pitch per lane so hits sound different.
   Order matches FSR_PINS: Left, Down, Up, Right */
const float LANE_TONES[NUM_LANES] = { 392.0f, 440.0f, 523.0f, 659.0f }; /* G4 A4 C5 E5 */

/* ---------------------------- FSR reading ---------------------------------- */

void checkFSRs() {
    unsigned long now = millis();

    for (int lane = 0; lane < NUM_LANES; lane++) {
        int reading  = analogRead(FSR_PINS[lane]);
        bool pressed = (reading > FSR_THRESHOLD);

        /* Rising edge: was not pressed, now is pressed, and cooldown elapsed */
        if (pressed && !wasPressed[lane] && (now - lastHitTime[lane] > HIT_COOLDOWN_MS)) {
            lastHitTime[lane] = now;

            /* Report the hit to the laptop over serial */
            Serial.print("HIT:");
            Serial.println(lane);

            /* Flash the LED for this pad */
            ledHit(lane);

            /* Play a short feedback tone through the speaker */
            playTone(LANE_TONES[lane], 120);
        }

        wasPressed[lane] = pressed;
    }
}

/* ---------------------------- Setup / Loop ---------------------------------- */

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("ESP32 DDR pad + speaker + LED firmware starting...");

    for (int i = 0; i < NUM_LANES; i++) {
        pinMode(FSR_PINS[i], INPUT);
    }

    ledInit();
    i2sInit();

    Serial.println("Ready. Waiting for pad presses...");
}

void loop() {
    checkFSRs();
    ledUpdate();

    /* Small delay keeps polling responsive without hammering the ADC.
       Tune down if hits feel laggy, tune up if readings are noisy. */
    delay(5);
}