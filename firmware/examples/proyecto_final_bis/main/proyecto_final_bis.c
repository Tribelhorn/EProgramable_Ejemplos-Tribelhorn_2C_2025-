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
#define NUMERO_0 48
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

uint8_t control_motor = 0;
uint16_t posicion = 0;

volatile uint16_t pasos_motor = 1200;

uint32_t PERIODO = 40000000;

TaskHandle_t preguntar_task_handle = NULL;	
TaskHandle_t mover_task_handle = NULL;

stepper_t motor = {
    .coilA = GPIO_22,
    .coilB = GPIO_21,
    .coilC = GPIO_20,
    .coilD = GPIO_19,
    .mode = STEPPER_FULL_STEP,
    .step_delay_ms = 15  /**< tiempo entre pasos en ms */
};

uint16_t casillas[14] ={9999,9999,9999,
                        9999,9999,9999,9999,
                        9999,9999,9999,9999,
                        9999,9999,9999 };

/*==================[internal functions declaration]=========================*/
/**
 * @brief Función a ejecutarse ante un interrupción de recepción 
 * a través de la conexión BLE.
 * 
 * @param data      Puntero a array de datos recibidos
 * @param length    Longitud del array de datos recibidos
 */


void PuestaACero(){
    while(GPIORead(GPIO_12)==1){
        vTaskDelay(10 / portTICK_PERIOD_MS);    
        StepperStep(&motor, 50);
        LedToggle(LED_2);
    }
}


static void tarea_preguntar(void *pvParameter) {
	while (true) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        RtcRead(&fecha_hora);
        uint16_t hora_militar = (fecha_hora.hour*100)+(fecha_hora.min);
        for (int i = 0; i < 14; i++) {
            if (casillas[i] == hora_militar) {
                control_motor = !control_motor;
                posicion = i+1;
            }
            else {
                PuestaACero();
            }
        }
	}
}

static void tarea_mover(void *pvParameter) {
    while (1) {    
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (control_motor) {
            BuzzerOn();        
            StepperStep(&motor, posicion*pasos_motor);
            uint16_t frec = 440;
            uint16_t tiempo = 3000;
            BuzzerPlayTone(frec, tiempo);
            BuzzerOff();
            control_motor = 0;
            posicion = 0;
        }
    }
}


void leer_info(uint8_t * data, uint8_t length){
    //Lee el horario de toma en formato (P-casilla-horario militar) = (P-CC-HHMM)/
    //O setea la hora actual como (A-horario militar) = (A-HHMM) //
    if (data [0] == LETRA_A && length >= 6) {
        uint16_t hora_actual = (data[2]-NUMERO_0)*10 + (data[3]-NUMERO_0); //Se le resta el 0 en ASCII
        uint16_t min_actual = (data[4]-NUMERO_0)*10 + (data[5]-NUMERO_0);
        fecha_hora.hour = hora_actual;
        fecha_hora.min = min_actual;
        RtcConfig(&fecha_hora);
    }

    else if (data[0] == LETRA_P && length >= 8) {
        uint16_t casilla = (((data[2]-NUMERO_0)*10)+(data[3]-NUMERO_0)-1); 
        uint16_t hora_pastilla = ( ((data[5]-NUMERO_0)*1000) + ((data[6]-NUMERO_0)*100) + ((data[7]-NUMERO_0)*10) + ((data[8]-NUMERO_0)) );
        casillas[casilla] = hora_pastilla;    
    }
}

void Fun_Timer(void* param){
    vTaskNotifyGiveFromISR(preguntar_task_handle, pdFALSE);		
    vTaskNotifyGiveFromISR(mover_task_handle, pdFALSE);
}

/*==================[external functions definition]==========================*/

void app_main(void){

    LedsInit();

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


	xTaskCreate(&tarea_preguntar, "Preguntar", 4096, NULL, 5, &preguntar_task_handle);
    xTaskCreate(&tarea_mover, "Mover", 4096, NULL, 5, &mover_task_handle);


    StepperInit(&motor);
    GPIOInit(GPIO_12, GPIO_INPUT);   //Sensor//

    PuestaACero();

    GPIOInit(GPIO_13, GPIO_OUTPUT);

    BuzzerInit(GPIO_13);

    


    BleInit(&ble_configuration);

    TimerInit(&timer_1);
  
    //* Inicialización del conteo de timers */
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
    }
}

/*==================[end of file]============================================*/
