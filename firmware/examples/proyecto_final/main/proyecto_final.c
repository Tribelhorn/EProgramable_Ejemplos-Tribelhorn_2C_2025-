/*! @mainpage ejemplo Bluetooth LED-RGB
 *
 * @section genDesc General Description
 *
 * Este proyecto ejemplifica el uso del módulo de comunicación Bluetooth Low Energy (BLE) 
 * junto con el manejo de tiras de LEDs RGB. 
 * Permite manejar la tonalidad e intensidad del LED RGB incluído en la placa ESP-EDU, 
 * mediante una aplicación móvil.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 02/04/2024 | Document creation		                         |
 *
 * @author Albano Peñalva (albano.penalva@uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led.h"
#include "ble_mcu.h"
#include "rtc_mcu.h"
#include "timer_mcu.h"
#include "stepper_24byj48.h"
#include "buzzer.h"
#include "gpio_mcu.h"

/*==================[macros and definitions]=================================*/

#define CONFIG_BLINK_PERIOD 1000
#define LED_BT	LED_1

#define LETRA_A 65
#define LETRA_P 80
/*==================[internal data definition]===============================*/

uint32_t primer_horario = 0000;

rtc_t fecha_hora = {
    .year = 2025,
    .month = 1,
    .mday = 1,
    .wday = 1,
    .hour = 0,
    .min = 0,
    .sec = 0,
};

volatile bool control_motor = true;

volatile uint16_t pasos_motor = 1200;

uint32_t PERIODO = 60000000;

TaskHandle_t motor_task_handle = NULL;	

stepper_t motor = {
    .coilA = GPIO_22,
    .coilB = GPIO_21,
    .coilC = GPIO_20,
    .coilD = GPIO_19,
    .mode = STEPPER_FULL_STEP,
    .step_delay_ms = 50  /**< tiempo entre pasos en ms */
};

uint16_t casillas[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/*==================[internal functions declaration]=========================*/
/**
 * @brief Función a ejecutarse ante un interrupción de recepción 
 * a través de la conexión BLE.
 * 
 * @param data      Puntero a array de datos recibidos
 * @param length    Longitud del array de datos recibidos
 */


static void tarea_preguntar(void *pvParameter) {
	while (true) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        RtcRead(&fecha_hora);
        StepperStep(&motor, pasos_motor);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
};

void leer_info(uint8_t * data, uint8_t length){
    //Lee el horario de toma en formato (P-casilla-horario militar) = (P-CC-HHMM)/
    //O setea la hora actual como (A-horario militar) = (A-HHMM) //
    if (data [0] == LETRA_A) {
        fecha_hora.hour = (data[2]-48)*10 + (data[3]);
        fecha_hora.min = (data[4]-48)*10 + (data[5]);
    }

    else if (data[0] == LETRA_P) {
        
    }

};

void PuestaACero(){
    while(GPIORead(GPIO_9)==1){
        vTaskDelay(10 / portTICK_PERIOD_MS);    
        StepperStep(&motor, 50);
    }
    StepperStep(&motor, 600);
}

void Fun_Timer(void* param){
    vTaskNotifyGiveFromISR(motor_task_handle, pdFALSE);		
}

/*==================[external functions definition]==========================*/

void app_main(void){

    RtcConfig(&fecha_hora);

    ble_config_t ble_configuration = {
        .device_name = "PASTILLERO",
        .func_p = leer_info  //Deberia ir directo a la función o una con vtask//
    };

    timer_config_t timer_1 = {				//Esta struct la definen en los drivers//
        .timer = TIMER_A,					//Hay 3 timers disponibles para usar
        .period = PERIODO,					//Periodo en microsegundos 
        .func_p = Fun_Timer,				//El nombre de una función es un puntero//
        .param_p = NULL
    };

	xTaskCreate(&tarea_preguntar, "Preguntar", 1024, NULL, 5, &motor_task_handle); //			//El 5 es la prioridad//

    StepperInit(&motor);
    GPIOInit(GPIO_13, GPIO_OUTPUT);

    BuzzerInit(GPIO_13);

    BuzzerOn();
    BuzzerPlayTone(440, 3000);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    BuzzerOff();

    GPIOInit(GPIO_9, GPIO_INPUT);
    LedsInit();
    BleInit(&ble_configuration);

    TimerInit(&timer_1);

    PuestaACero();
 
    /* Inicialización del conteo de timers */
    TimerStart(timer_1.timer);

    while(1){
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
        switch(BleStatus()) {
            case BLE_OFF:
                LedOff(LED_BT);
            break;
            case BLE_DISCONNECTED:
                LedToggle(LED_BT);
            break;
            case BLE_CONNECTED:
                LedOn(LED_BT);
            break;
        }
    };
}

/*==================[end of file]============================================*/
