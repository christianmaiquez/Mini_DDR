/* ============================================================================
   ESP32 FIRMWARE — FSR dance pad + looping I2S music + LEDs
   ----------------------------------------------------------------------------
   Responsibilities of this board:
     1. Read 4 FSR pressure pads (Left/Down/Up/Right)
     2. Send pad hits to the laptop as "HIT:x\n"
     3. Play a looping generated dance track through a MAX98357A while the
        laptop game is running
     4. Mix short lane hit sounds over the background music
     5. Flash the corresponding physical LED for FSR and keyboard presses

   Laptop -> ESP32 serial commands:
     MUSIC:START    Start/restart the background track
     MUSIC:STOP     Stop the background track
     FEEDBACK:x     Flash lane x LED and play its hit sound (keyboard input)

   Wiring:
     FSR Left   -> GPIO34 (ADC1_CH6)
     FSR Down   -> GPIO35 (ADC1_CH7)
     FSR Up     -> GPIO32 (ADC1_CH4)
     FSR Right  -> GPIO33 (ADC1_CH5)

     I2S Amp (MAX98357A):
     BCLK -> GPIO26
     LRC  -> GPIO25
     DOUT -> GPIO22

     LEDs:
     LED Left  -> GPIO14
     LED Down  -> GPIO13
     LED Up    -> GPIO27
     LED Right -> GPIO4
   ============================================================================ */

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

/* ---------------------------- Hardware config ----------------------------- */

#define I2S_PORT    I2S_NUM_0
#define I2S_BCLK    26
#define I2S_LRC     25
#define I2S_DOUT    22
#define SAMPLE_RATE 22050

#define NUM_LANES 4

const int FSR_PINS[NUM_LANES] = {33, 32, 34, 35};
const int LED_PINS[NUM_LANES] = {14, 13, 27, 4};

#define FSR_THRESHOLD   1200
#define HIT_COOLDOWN_MS 150
#define LED_FLASH_MS    150

/* Keep the summed audio comfortably below the int16 range. */
#define MUSIC_MELODY_VOLUME 3000.0f
#define MUSIC_BASS_VOLUME   1800.0f
#define HIT_VOLUME          4800.0f

/* ---------------------------- Pad / LED state ------------------------------ */

unsigned long lastHitTime[NUM_LANES] = {0, 0, 0, 0};
bool wasPressed[NUM_LANES] = {false, false, false, false};
unsigned long ledOnTime[NUM_LANES] = {0, 0, 0, 0};

void ledInit() {
    for (int lane = 0; lane < NUM_LANES; lane++) {
        pinMode(LED_PINS[lane], OUTPUT);
        digitalWrite(LED_PINS[lane], LOW);
    }
}

void ledHit(int lane) {
    if (lane < 0 || lane >= NUM_LANES) return;
    digitalWrite(LED_PINS[lane], HIGH);
    ledOnTime[lane] = millis();
}

void ledUpdate() {
    const unsigned long now = millis();
    for (int lane = 0; lane < NUM_LANES; lane++) {
        if (ledOnTime[lane] != 0 && now - ledOnTime[lane] >= LED_FLASH_MS) {
            digitalWrite(LED_PINS[lane], LOW);
            ledOnTime[lane] = 0;
        }
    }
}

/* ---------------------------- Music sequencer ------------------------------ */

/* The audio task is the only code that writes to I2S. This prevents the
   background track and hit sounds from fighting over the amplifier. */
static QueueHandle_t hitSoundQueue = NULL;
static volatile bool musicPlaying = false;
static volatile bool musicRestartRequested = false;

/* Frequencies in Hz. A zero is a rest. The 32 eighth-note steps form a
   simple original chiptune loop, so no external audio file is required. */
static const float MELODY[32] = {
    659.25f, 0.0f, 783.99f, 0.0f, 880.00f, 783.99f, 659.25f, 523.25f,
    587.33f, 0.0f, 659.25f, 0.0f, 783.99f, 659.25f, 587.33f, 493.88f,
    523.25f, 0.0f, 659.25f, 0.0f, 783.99f, 880.00f, 783.99f, 659.25f,
    587.33f, 659.25f, 523.25f, 493.88f, 440.00f, 493.88f, 523.25f, 587.33f
};

static const float BASS[32] = {
    130.81f, 0.0f, 130.81f, 0.0f, 130.81f, 0.0f, 130.81f, 0.0f,
    110.00f, 0.0f, 110.00f, 0.0f, 110.00f, 0.0f, 110.00f, 0.0f,
    87.31f,  0.0f, 87.31f,  0.0f, 87.31f,  0.0f, 87.31f,  0.0f,
    98.00f,  0.0f, 98.00f,  0.0f, 98.00f,  0.0f, 98.00f,  0.0f
};

static const float LANE_TONES[NUM_LANES] = {
    392.00f, 440.00f, 523.25f, 659.25f
};

static int16_t clampSample(float sample) {
    if (sample > 32767.0f) return 32767;
    if (sample < -32768.0f) return -32768;
    return (int16_t)sample;
}

bool i2sInit() {
    const i2s_config_t i2sConfig = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = 128,
        .use_apll             = false
    };

    const i2s_pin_config_t pinConfig = {
        .bck_io_num   = I2S_BCLK,
        .ws_io_num    = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2sConfig, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("i2s_driver_install failed: %d\n", err);
        return false;
    }

    err = i2s_set_pin(I2S_PORT, &pinConfig);
    if (err != ESP_OK) {
        Serial.printf("i2s_set_pin failed: %d\n", err);
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    i2s_zero_dma_buffer(I2S_PORT);
    return true;
}

void musicSetPlaying(bool shouldPlay) {
    if (shouldPlay) {
        musicRestartRequested = true;
        musicPlaying = true;
    } else {
        musicPlaying = false;
    }
}

void triggerHitSound(int lane) {
    if (!hitSoundQueue || lane < 0 || lane >= NUM_LANES) return;
    xQueueSend(hitSoundQueue, &lane, 0);
}

void audioTask(void *parameter) {
    (void)parameter;

    const int bufferSamples = 128;
    int16_t buffer[bufferSamples];

    /* 120 BPM, eighth-note sequence. */
    const uint32_t samplesPerStep = (uint32_t)(SAMPLE_RATE * 60.0f / 120.0f / 2.0f);
    const uint32_t hitLengthSamples = (uint32_t)(SAMPLE_RATE * 0.10f);

    uint32_t stepSample = 0;
    int stepIndex = 0;
    float melodyPhase = 0.0f;
    float bassPhase = 0.0f;

    float hitPhase[NUM_LANES] = {0, 0, 0, 0};
    uint32_t hitSamplesLeft[NUM_LANES] = {0, 0, 0, 0};

    for (;;) {
        if (musicRestartRequested) {
            stepSample = 0;
            stepIndex = 0;
            melodyPhase = 0.0f;
            bassPhase = 0.0f;
            musicRestartRequested = false;
        }

        /* Drain queued lane sounds. Different lanes can overlap, so chords
           still sound and no blocking tone function delays FSR polling. */
        int queuedLane = -1;
        while (xQueueReceive(hitSoundQueue, &queuedLane, 0) == pdTRUE) {
            if (queuedLane >= 0 && queuedLane < NUM_LANES) {
                hitPhase[queuedLane] = 0.0f;
                hitSamplesLeft[queuedLane] = hitLengthSamples;
            }
        }

        for (int i = 0; i < bufferSamples; i++) {
            float sample = 0.0f;

            if (musicPlaying) {
                const float melodyFreq = MELODY[stepIndex];
                const float bassFreq = BASS[stepIndex];
                const float position = (float)stepSample / (float)samplesPerStep;

                /* Fast attack and gentle decay reduce clicks at note edges. */
                float envelope = 1.0f - 0.55f * position;
                if (position < 0.03f) envelope *= position / 0.03f;

                if (melodyFreq > 0.0f) {
                    sample += MUSIC_MELODY_VOLUME * envelope * sinf(melodyPhase);
                    melodyPhase += 2.0f * PI * melodyFreq / SAMPLE_RATE;
                    if (melodyPhase >= 2.0f * PI) melodyPhase -= 2.0f * PI;
                }

                if (bassFreq > 0.0f) {
                    sample += MUSIC_BASS_VOLUME * sinf(bassPhase);
                    bassPhase += 2.0f * PI * bassFreq / SAMPLE_RATE;
                    if (bassPhase >= 2.0f * PI) bassPhase -= 2.0f * PI;
                }

                stepSample++;
                if (stepSample >= samplesPerStep) {
                    stepSample = 0;
                    stepIndex = (stepIndex + 1) % 32;
                    melodyPhase = 0.0f;
                    bassPhase = 0.0f;
                }
            }

            for (int lane = 0; lane < NUM_LANES; lane++) {
                if (hitSamplesLeft[lane] == 0) continue;

                const float progress =
                    1.0f - (float)hitSamplesLeft[lane] / (float)hitLengthSamples;
                const float hitEnvelope = 1.0f - progress;
                sample += HIT_VOLUME * hitEnvelope * sinf(hitPhase[lane]);

                hitPhase[lane] += 2.0f * PI * LANE_TONES[lane] / SAMPLE_RATE;
                if (hitPhase[lane] >= 2.0f * PI) hitPhase[lane] -= 2.0f * PI;
                hitSamplesLeft[lane]--;
            }

            buffer[i] = clampSample(sample);
        }

        size_t bytesWritten = 0;
        i2s_write(
            I2S_PORT,
            buffer,
            sizeof(buffer),
            &bytesWritten,
            portMAX_DELAY
        );
    }
}

void audioInit() {
    if (!i2sInit()) {
        Serial.println("I2S audio unavailable.");
        return;
    }

    hitSoundQueue = xQueueCreate(12, sizeof(int));
    if (!hitSoundQueue) {
        Serial.println("Could not create hit sound queue.");
        return;
    }

    BaseType_t result = xTaskCreatePinnedToCore(
        audioTask,
        "ddr_audio",
        4096,
        NULL,
        2,
        NULL,
        0
    );

    if (result != pdPASS) {
        Serial.println("Could not start audio task.");
    }
}

/* ---------------------------- Serial commands ----------------------------- */

static char serialCommand[64];
static size_t serialCommandLength = 0;

void processSerialCommand(const char *command) {
    if (strcmp(command, "MUSIC:START") == 0) {
        musicSetPlaying(true);
        Serial.println("MUSIC_STARTED");
        return;
    }

    if (strcmp(command, "MUSIC:STOP") == 0) {
        musicSetPlaying(false);
        Serial.println("MUSIC_STOPPED");
        return;
    }

    int lane = -1;
    if (sscanf(command, "FEEDBACK:%d", &lane) == 1 &&
        lane >= 0 && lane < NUM_LANES) {
        ledHit(lane);
        triggerHitSound(lane);
    }
}

void checkSerialCommands() {
    while (Serial.available() > 0) {
        const char ch = (char)Serial.read();

        if (ch == '\r') continue;

        if (ch == '\n') {
            serialCommand[serialCommandLength] = '\0';
            if (serialCommandLength > 0) {
                processSerialCommand(serialCommand);
            }
            serialCommandLength = 0;
        } else if (serialCommandLength < sizeof(serialCommand) - 1) {
            serialCommand[serialCommandLength++] = ch;
        } else {
            serialCommandLength = 0;
        }
    }
}

/* ---------------------------- FSR reading --------------------------------- */

void checkFSRs() {
    const unsigned long now = millis();

    for (int lane = 0; lane < NUM_LANES; lane++) {
        const int reading = analogRead(FSR_PINS[lane]);
        const bool pressed = reading > FSR_THRESHOLD;

        if (pressed && !wasPressed[lane] &&
            now - lastHitTime[lane] > HIT_COOLDOWN_MS) {
            lastHitTime[lane] = now;

            Serial.print("HIT:");
            Serial.println(lane);

            ledHit(lane);
            triggerHitSound(lane);
        }

        wasPressed[lane] = pressed;
    }
}

/* ---------------------------- Setup / loop -------------------------------- */

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("ESP32 DDR pad + music + LED firmware starting...");

    for (int lane = 0; lane < NUM_LANES; lane++) {
        pinMode(FSR_PINS[lane], INPUT);
    }

    ledInit();
    audioInit();

    Serial.println("Ready. Choose a difficulty on the laptop to start music.");
}

void loop() {
    checkSerialCommands();
    checkFSRs();
    ledUpdate();
    delay(2);
}
