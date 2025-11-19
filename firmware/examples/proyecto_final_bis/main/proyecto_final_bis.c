/**
 * @file proyecto_final_bis.c
 * @brief Firmware del pastillero automático con ESP32-C6, BLE, RTC, motor paso a paso, sensor FC-123 y buzzer.
 *
 * 
 * Implementa:
 * - Recepción de horarios por BLE
 * - Configuración de hora RTC
 * - Activación de motor para entregar pastillas
 * - Aviso acústico con buzzer
 * - Detección de retiro de pastilla
 * - Posicionamiento a cero mediante un sensor
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
#include "uart_mcu.h"
#include "switch.h"

/*==================[macros and definitions]=================================*/

#define CONFIG_BLINK_PERIOD 1000
#define LED_BT	LED_1

/** @brief Código ASCII de la letra 'A' */
#define LETRA_A 65

/** @brief Código ASCII de la letra 'P' */
#define LETRA_P 80

/** @brief Código ASCII del numero '0' */
#define NUMERO_0 48

/*==================[internal data definition]===============================*/

/** @brief Estructura RTC con fecha inicial arbitraria. */
rtc_t fecha_hora = {
    .year = 2025,
    .month = 1,
    .mday = 1,
    .wday = 1,
    .hour = 0,
    .min = 0,
    .sec = 0,
};

/** @brief Casilla seleccionada para entregar la pastilla (Inicialmente en la posición 0). */
uint16_t posicion = 0;

/** @brief Cantidad de pasos por casilla. */
volatile uint16_t pasos_motor = 1000;

/** @brief Período del timer en microsegundos (un minuto). */
uint32_t PERIODO = 60000000;

/** @brief Buffer para enviar mensajes por Bluetooth. */
char buffer[7];  //Para enviar la cadena al BT//

/** @brief Variable para almacenar el estado de los switches. */
uint16_t teclas;

/** @brief Handle de la tarea encargada de verificar horarios. */
TaskHandle_t preguntar_task_handle = NULL;	

/** @brief Handle de la tarea encargada del movimiento del motor. */
TaskHandle_t mover_task_handle = NULL;

/** @brief Configuración del motor paso a paso. */
stepper_t motor = {
    .coilA = GPIO_22,
    .coilB = GPIO_21,
    .coilC = GPIO_20,
    .coilD = GPIO_19,
    .mode = STEPPER_FULL_STEP,
    .step_delay_ms = 10  /**< tiempo entre pasos en ms */
};

/**
 * @brief Horarios configurados para cada casilla en formato HHMM.
 * (Se inicializan en 9999 para indicar "vacío").
 */
uint16_t casillas[14] ={9999,9999,9999,
                        9999,9999,9999,9999,
                        9999,9999,9999,9999,
                        9999,9999,9999 };

/*==================[internal functions declaration]=========================*/

/**
 * @brief Lleva el motor a la posición de referencia (cero) usando el sensor en el GPIO_12.
 */
void PuestaACero(){
    while(GPIORead(GPIO_12)==1){
        vTaskDelay(10 / portTICK_PERIOD_MS);    
        StepperStep(&motor, 50);
    }
}

/**
 * @brief Tarea encargada de revisar la hora actual y compararla con los horarios programados.
 *
 * @param pvParameter No utilizado.
 */
static void tarea_preguntar(void *pvParameter) {
	while (true) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        RtcRead(&fecha_hora);
        uint16_t hora_militar = (fecha_hora.hour*100)+(fecha_hora.min);
        for (int i = 0; i < 14; i++) {
            if (casillas[i] == hora_militar) {
                posicion = i+1;
                xTaskNotifyGive(mover_task_handle);
            }
        }
	}
}

/**
 * @brief Llamada en interrupción del switch.  
 * Permite la puesta a cero manual.
 */
void leer_switches(void) {
	teclas  = SwitchesRead();
    switch(teclas){
    	case SWITCH_1:    			
            PuestaACero();
    }
}

/**
 * @brief Tarea encargada de accionar el motor y emitir alertas cuando corresponde entregar pastillas.
 *
 * @param pvParameter No utilizado.
 */
static void tarea_mover(void *pvParameter) {
    while (1) {    
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        PuestaACero();
        BuzzerOn();        
        StepperStep(&motor, posicion*pasos_motor);
        uint16_t frec = 440;
        uint16_t tiempo = 3000;
        BuzzerPlayTone(frec, tiempo);
        BuzzerOff();
        BleSendString("Hora de la pastilla ");
        sprintf(buffer, "%u", posicion);
        BleSendString(buffer);
        BleSendString("\nPresione la tecla 1 cuando retire la pastilla \n");
    }
}


/**
 * @brief Recepción de datos por BLE
 * 
 * @param data      Puntero a array de datos recibidos
 * @param length    Longitud del array de datos recibidos
 */
void leer_info(uint8_t * data, uint8_t length){
    //Lee el horario de toma en formato (P-casilla-horario militar) = (P-CC-HHMM)/
    //O setea la hora actual como (A-horario militar) = (A-HHMM) //
    if (data [0] == LETRA_A && length >= 6) {
        uint16_t hora_actual = (data[2]-NUMERO_0)*10 + (data[3]-NUMERO_0); //Se le resta el 0 en ASCII
        uint16_t min_actual = (data[4]-NUMERO_0)*10 + (data[5]-NUMERO_0);
        fecha_hora.hour = hora_actual;
        fecha_hora.min = min_actual;
        RtcConfig(&fecha_hora);
        BleSendString("Hora configurada \n");
    }

    else if (data[0] == LETRA_P && length >= 9) {
        uint16_t casilla = (((data[2]-NUMERO_0)*10)+(data[3]-NUMERO_0)-1); 
        uint16_t hora_pastilla = ( ((data[5]-NUMERO_0)*1000) + ((data[6]-NUMERO_0)*100) + ((data[7]-NUMERO_0)*10) + ((data[8]-NUMERO_0)) );
        casillas[casilla] = hora_pastilla;    
        BleSendString("Horario recibido \n");
    }
}

/**
 * @brief Funcion a la cual llama periódicamente el timer.  
 * Notifica a la tarea de verificación de horarios.
 *
 * @param param Parámetro no utilizado.
 */
void Fun_Timer(void* param){
    vTaskNotifyGiveFromISR(preguntar_task_handle, pdFALSE);		
}

/*==================[external functions definition]==========================*/


/**
 * @brief Inicializa todos los periféricos, tareas, BLE, RTC y timer.
 */
void app_main(void){

    LedsInit();

    RtcConfig(&fecha_hora);

    ble_config_t ble_configuration = {
        .device_name = "PASTILLERO",
        .func_p = leer_info  
    };

    timer_config_t timer_1 = {				//Esta struct la definen en los drivers//
        .timer = TIMER_A,					//Hay 3 timers disponibles para usar
        .period = PERIODO,					//Periodo en microsegundos 
        .func_p = Fun_Timer,				//El nombre de una función es un puntero//
        .param_p = NULL
    };


	xTaskCreate(&tarea_preguntar, "Preguntar", 8192, NULL, 5, &preguntar_task_handle);
    xTaskCreate(&tarea_mover, "Mover", 8192, NULL, 5, &mover_task_handle);

    SwitchesInit();
    SwitchActivInt(SWITCH_1, leer_switches, NULL);

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
