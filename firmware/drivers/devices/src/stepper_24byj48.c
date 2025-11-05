#include "stepper_24byj48.h"
#include "delay_mcu.h"
#include "freertos/FreeRTOS.h"

static const uint8_t full_step_seq[4] = {
    0b0001, // A
    0b0010, // B
    0b0100, // C
    0b1000  // D
};

static const uint8_t half_step_seq[8] = {
    0b0001, // A
    0b0011, // A+B
    0b0010, // B
    0b0110, // B+C
    0b0100, // C
    0b1100, // C+D
    0b1000, // D
    0b1001  // D+A
};

static void set_coils(stepper_t *m, uint8_t mask) {
    GPIOState(m->coilA, mask & 0x01);
    GPIOState(m->coilB, mask & 0x02);
    GPIOState(m->coilC, mask & 0x04);
    GPIOState(m->coilD, mask & 0x08);
}

void StepperInit(stepper_t *m) {
    GPIOInit(m->coilA, GPIO_OUTPUT);
    GPIOInit(m->coilB, GPIO_OUTPUT);
    GPIOInit(m->coilC, GPIO_OUTPUT);
    GPIOInit(m->coilD, GPIO_OUTPUT);
    StepperOff(m);
    if (m->step_delay_ms == 0)
        m->step_delay_ms = 5; // valor por defecto
}

void StepperSetSpeed(stepper_t *m, uint16_t delay_ms) {
    m->step_delay_ms = delay_ms;
}

void StepperStep(stepper_t *m, int32_t steps) {
    const uint8_t *seq;
    uint8_t len;
    if (m->mode == STEPPER_FULL_STEP) {
        seq = full_step_seq;
        len = 4;
    } else {
        seq = half_step_seq;
        len = 8;
    }

    int dir = (steps >= 0) ? 1 : -1;
    if (steps < 0) steps = -steps;

    int idx = 0;

    for (int32_t s = 0; s < steps; s++) {
        set_coils(m, seq[idx]);
        vTaskDelay(m->step_delay_ms / portTICK_PERIOD_MS);
        idx = (idx + dir + len) % len;
    }

    StepperOff(m);
}

void StepperOff(stepper_t *m) {
    GPIOOff(m->coilA);
    GPIOOff(m->coilB);
    GPIOOff(m->coilC);
    GPIOOff(m->coilD);
}
