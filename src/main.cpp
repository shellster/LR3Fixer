#include <Arduino.h>

// === Pin Configuration (Digispark ATtiny85 / ATTinyCore) ===
//
// Pin map:  D0=PB0=P0  D1=PB1=P1  D2=PB2=P2  D3=PB3=P3  D4=PB4=P4  D5=PB5=P5(RESET)
//
// INPUT:  P2 (D2, PB2) — Safest input pin. Completely untouched by the
//         Micronucleus USB bootloader during its 5-6 second boot wait.
//         No pullup/pulldown is asserted by the bootloader. The pin floats
//         undisturbed, letting external circuitry control it cleanly.
//
// OUTPUT: P0 (D0, PB0) — Never driven HIGH by the bootloader. The bootloader
//         only reads it (jumper entry mode) with no internal pullup enabled,
//         so it remains floating. We drive it LOW explicitly in setup().
//         *** EXTERNAL 10K PULL-DOWN TO GND REQUIRED on P0 ***
//         Without it, the pin floats and could briefly read HIGH through
//         capacitance coupling, causing an unintended trigger.
//
// LED:    P1 (D1, PB1) — Onboard LED. The bootloader toggles this pin
//         during its boot wait, but once setup() runs, we have full control.
//         We turn it ON (LOW = active) when the blink pattern is detected.

#define INPUT_PIN   2
#define OUTPUT_PIN  0
#define LED_PIN     1

// === Timing Configuration (milliseconds) ===

// NOTE:  These are edge crossing so two per "blink" 
#define MIN_EDGE_INTERVAL_MS    250UL
#define MAX_EDGE_INTERVAL_MS    1000UL

#define HIGH_STEADY_TIMEOUT_MS  5000UL
#define LOW_STEADY_TIMEOUT_MS   5000UL
#define REQUIRED_BLINKS         15
#define COOLDOWN_MS             900000UL

// === Output Pulse Configuration (milliseconds) ===
#define PULSE_HIGH_1_MS         1000UL
#define PULSE_LOW_1_MS          3000UL
#define PULSE_HIGH_2_MS         1000UL

// === Output Sequence States ===
enum OutState : uint8_t {
    OUT_DETECTING,
    OUT_HIGH_1,
    OUT_LOW_1,
    OUT_HIGH_2,
    OUT_COOLDOWN
};

// === State Variables ===
static bool prev_state = LOW;
static unsigned long last_edge_time = 0;
static unsigned long blink_count = 0;
static bool first_edge = true;
static OutState out_state = OUT_DETECTING;
static unsigned long out_phase_start = 0;
static bool blink_state = LOW;

void setup()
{
    pinMode(OUTPUT_PIN, OUTPUT);
    digitalWrite(OUTPUT_PIN, LOW);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    pinMode(INPUT_PIN, INPUT);

    prev_state = digitalRead(INPUT_PIN);
    last_edge_time = millis();
    first_edge = true;
    blink_state = LOW;
}

static void reset_detection()
{
    blink_count = 0;
    prev_state = digitalRead(INPUT_PIN);
    last_edge_time = millis();
    out_state = OUT_DETECTING;
    first_edge = true;
}

static void advance_output_sequence(unsigned long now_ms)
{
    switch (out_state) {
        case OUT_HIGH_1:
            if (now_ms - out_phase_start >= PULSE_HIGH_1_MS) {
                digitalWrite(OUTPUT_PIN, LOW);
                digitalWrite(LED_PIN, LOW);
                out_state = OUT_LOW_1;
                out_phase_start = now_ms;
            }
            break;
        case OUT_LOW_1:
            if (now_ms - out_phase_start >= PULSE_LOW_1_MS) {
                digitalWrite(OUTPUT_PIN, HIGH);
                digitalWrite(LED_PIN, HIGH);
                out_state = OUT_HIGH_2;
                out_phase_start = now_ms;
            }
            break;
        case OUT_HIGH_2:
            if (now_ms - out_phase_start >= PULSE_HIGH_2_MS) {
                digitalWrite(OUTPUT_PIN, LOW);
                digitalWrite(LED_PIN, LOW);
                out_state = OUT_COOLDOWN;
                out_phase_start = now_ms;
            }
            break;
        case OUT_COOLDOWN:
            if (now_ms - out_phase_start >= COOLDOWN_MS) {
                reset_detection();
            }
            break;
        default:
            break;
    }
}

void loop()
{
    unsigned long now_ms = millis();

    if (out_state != OUT_DETECTING) {
        advance_output_sequence(now_ms);
        return;
    }

    bool current_state = digitalRead(INPUT_PIN);

    if (current_state != prev_state) {
        unsigned long edge_interval = now_ms - last_edge_time;
        last_edge_time = now_ms;
        prev_state = current_state;

        if (!first_edge) {
            if (edge_interval >= MIN_EDGE_INTERVAL_MS && edge_interval <= MAX_EDGE_INTERVAL_MS) {
                blink_state = !blink_state;
                digitalWrite(LED_PIN, blink_state);
                
                blink_count++;
                if (blink_count >= REQUIRED_BLINKS) {
                    digitalWrite(OUTPUT_PIN, HIGH);
                    digitalWrite(LED_PIN, HIGH);
                    out_state = OUT_HIGH_1;
                    out_phase_start = now_ms;
                    first_edge = true;
                    return;
                }
            }
        } else {
            first_edge = false;
        }
    }

    unsigned long time_in_state = now_ms - last_edge_time;
    if ((current_state == HIGH && time_in_state > HIGH_STEADY_TIMEOUT_MS) ||
        (current_state == LOW  && time_in_state > LOW_STEADY_TIMEOUT_MS)) {
        blink_count = 0;
    }
}
