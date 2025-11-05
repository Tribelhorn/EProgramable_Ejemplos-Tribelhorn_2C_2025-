/**
 * @file stepper_mcu.h
 * @brief Driver de motor paso a paso usando GPIO y Delay MCU
 * @author Juan
 */

#ifndef STEPPER_MCU_H
#define STEPPER_MCU_H

#include <stdint.h>
#include <stdbool.h>
#include "gpio_mcu.h"

/**
 * @brief Tipos de secuencia
 */
typedef enum {
    STEPPER_FULL_STEP = 0,  /**< Secuencia completa (4 pasos) */
    STEPPER_HALF_STEP       /**< Secuencia intermedia (8 pasos) */
} stepper_mode_t;

/**
 * @brief Estructura de configuración del motor paso a paso
 */
typedef struct {
    gpio_t coilA;
    gpio_t coilB;
    gpio_t coilC;
    gpio_t coilD;
    stepper_mode_t mode;
    uint16_t step_delay_ms;   /**< tiempo entre pasos en ms */
} stepper_t;

void StepperInit(stepper_t *motor);
void StepperSetSpeed(stepper_t *motor, uint16_t delay_ms);
void StepperStep(stepper_t *motor, int32_t steps);
void StepperOff(stepper_t *motor);

#endif // STEPPER_MCU_H
