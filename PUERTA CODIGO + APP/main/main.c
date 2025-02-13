
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#define ESTADO_INICIAL 0
#define ESTADO_ERROR 1
#define ESTADO_ABRIENDO 2
#define ESTADO_CERRANDO 3
#define ESTADO_ABIERTA 4
#define ESTADO_CERRADA 5
#define ESTADO_DETENIDA 6
#define ESTADO_DESCONOCIDO 7

#define LM_ACTIVO 1
#define LM_INACTIVO 0
#define PP_ACTIVO 1
#define PP_INACTIVO 0
#define FTC_ACTIVO 1
#define FTC_INACTIVO 0
#define KEY_ACTIVO 1
#define KEY_INACTIVO 0
#define MOTOR_ON 1
#define MOTOR_OFF 0
#define LAMP_ON 1
#define LAMP_OFF 0
#define BUZZER_ON 1
#define BUZZER_OFF 0

#define FDESCONOCIDO_CIERRA 0
#define FDESCONOCIDO_ABRIR 1
#define FDESCONOCIDO_ESPERA 2
#define FDESCONOCIDO_PARPADEAR 3

#define FDETENIDA_SEGUIR 0
#define FDETENIDA_CONTRARIO 1

// se definen los pines de entrada y salida
#define pin_lsc 2 // limit switch puerta cerrada
#define pin_lsa 4  // limit switch puerta abierta
#define pin_ftc 5 // fotocelda
#define pin_keya 18 
#define pin_keyc 19
#define pin_pp 21
#define pin_dpsw_DESCONOCIDO_LSB 27// dpsw de dos pines para funcionamiento a partir de desconocido y boton pp desde estado desconocido
#define pin_dpsw_DESCONOCIDO_MSB 14 
#define pin_dpsw_DETENIDA_LSB 12
#define pin_dpsw_DETENIDA_MSB 13
#define pin_ma 26  // motor abrir
#define pin_mc 25   // motor cerrar
#define pin_lamp 32 // lampara
#define pin_buzzer 34

int Func_SETTING_IO(void);

int Func_ESTADO_INICIAL(void);
int Func_ESTADO_ERROR(void);
int Func_ESTADO_ABRIENDO(void);
int Func_ESTADO_CERRANDO(void);
int Func_ESTADO_ABIERTA(void);
int Func_ESTADO_CERRADA(void);
int Func_ESTADO_DETENIDA(void);
int Func_ESTADO_DESCONOCIDO(void);

int ESTADO_SIGUIENTE = ESTADO_INICIAL;
int ESTADO_ANTERIOR = ESTADO_INICIAL;
int ESTADO_ACTUAL = ESTADO_INICIAL;

static const char *tag = "Main";

/*
funcionamiento determinado por dpsw
Desde ESTADO_DESCONOCIDO:
00 seguir
01 contrario

Desde ESTADO_DETENIDA:
00 cerrar
01 abrir
10 esperar
11 parpadear
*/

struct SYSTEM_IO
{
    unsigned int lsc : 1;  // limit switch puerta cerrada
    unsigned int lsa : 1;  // limit switch puerta abierta
    unsigned int ftc : 1;  // fotocelda
    unsigned int ma : 1;   // motor abrir
    unsigned int mc : 1;   // motor cerrar
    unsigned int lamp : 1; // lampara
    unsigned int buzzer : 1;
    unsigned int keya : 1;
    unsigned int keyc : 1;
    unsigned int pp : 1;
    unsigned int dpsw_DESCONOCIDO : 2; // dpsw de dos pines para funcionamiento a partir de desconocido y boton pp desde estado desconocido
    unsigned int dpsw_DETENIDA : 2;
} io;

struct SYSTEM_CONFIG
{
    unsigned int cnt_TCA; // contador en segundos del tiempo de cierre automatico
    unsigned int cnt_RT; // contador en segundos del tiempo maximo de movimiento del motor
    int FDESCONOCIDO; 
    int FDETENIDA; // contiene la configuracion del modo detenido
} config;

//timer
TimerHandle_t xTimers;
int x; 

void vTimerCallback(TimerHandle_t pxTimer)
{
/*pin_lsc 2 // limit switch puerta cerrada
pin_lsa 4  // limit switch puerta abierta
pin_ftc 5 // fotocelda
pin_keya 18 
pin_keyc 19
pin_pp 21
pin_dpsw_DESCONOCIDO_LSB 27// dpsw de dos pines para funcionamiento a partir de desconocido y boton pp desde estado desconocido
pin_dpsw_DESCONOCIDO_MSB 14 
pin_dpsw_DETENIDA_LSB 12
pin_dpsw_DETENIDA_MSB 13
pin_ma 26  // motor abrir
pin_mc 25   // motor cerrar
pin_lamp 32 // lampara
pin_buzzer 34*/

    ESP_LOGI(tag, "Event was called from timer");
    if(gpio_get_level(pin_lsc))
    {
        io.lsc = LM_ACTIVO;
    }
    else
    {
        io.lsc = LM_INACTIVO;
    }

    if(gpio_get_level(pin_lsa))
    {
        io.lsa = LM_ACTIVO;
    }
    else
    {
        io.lsa = LM_INACTIVO;
    }

    if(gpio_get_level(pin_ftc))
    {
        io.ftc = FTC_ACTIVO;
    }
    else
    {
        io.ftc = FTC_INACTIVO;
    }

    if(gpio_get_level(pin_keya))
    {
        io.keya = KEY_ACTIVO;
    }
    else
    {
        io.keya = KEY_INACTIVO;
    }

    if(gpio_get_level(pin_keyc))
    {
        io.keyc = KEY_ACTIVO;
    }
    else
    {
        io.keyc = KEY_INACTIVO;
    }

    if(gpio_get_level(pin_pp))
    {
        io.pp = PP_ACTIVO;
    }
    else
    {
        io.pp = PP_INACTIVO;
    }

    int bitDS0;
    int bitDS1;

    if(gpio_get_level(pin_dpsw_DESCONOCIDO_LSB))
    {
        bitDS0 = 1;
    }

    else
    {
        bitDS0 = 0;
    }

    if(gpio_get_level(pin_dpsw_DESCONOCIDO_MSB))
    {
        bitDS1 = 1;
    }

    else
    {
        bitDS1 = 0;
    }

    io.dpsw_DESCONOCIDO = (bitDS1 << 1) | bitDS0;

    int bitDT0;
    int bitDT1;

    if(gpio_get_level(pin_dpsw_DETENIDA_LSB))
    {
        bitDT0 = 1;
    }

    else
    {
        bitDT0 = 0;
    }

    if(gpio_get_level(pin_dpsw_DETENIDA_MSB))
    {
        bitDT1 = 1;
    }

    else
    {
        bitDT1 = 0;
    }

    io.dpsw_DETENIDA = (bitDT1 << 1) | bitDT0;

    if(io.ma == MOTOR_ON)
    {
        gpio_set_level(pin_ma, MOTOR_ON);
    }

    else 
    {
        gpio_set_level(pin_ma, MOTOR_OFF);
    }

    if(io.mc == MOTOR_ON)
    {
        gpio_set_level(pin_mc, MOTOR_ON);
    }

    else 
    {
        gpio_set_level(pin_mc, MOTOR_OFF);
    }

    if(io.lamp == LAMP_ON)
    {
        gpio_set_level(pin_lamp, LAMP_ON);
    }

    else 
    {
        gpio_set_level(pin_lamp, LAMP_OFF);
    }

    if(io.buzzer == BUZZER_ON)
    {
        gpio_set_level(pin_buzzer, BUZZER_ON);
    }

    else
    {
        gpio_set_level(pin_buzzer, BUZZER_OFF); 
    }
}

int set_timer(void);

void app_main(void)
{
    Func_SETTING_IO();
    set_timer();

    for (;;)
    {
        if (ESTADO_SIGUIENTE == ESTADO_INICIAL)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_INICIAL();
        }

        if (ESTADO_SIGUIENTE == ESTADO_ERROR)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_ERROR();
        }

        if (ESTADO_SIGUIENTE == ESTADO_ABRIENDO)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_ABRIENDO();
        }

        if (ESTADO_SIGUIENTE == ESTADO_CERRANDO)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_CERRANDO();
        }

        if (ESTADO_SIGUIENTE == ESTADO_ABIERTA)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_ABIERTA();
        }

        if (ESTADO_SIGUIENTE == ESTADO_CERRADA)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_CERRADA();
        }

        if (ESTADO_SIGUIENTE == ESTADO_DETENIDA)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_DETENIDA();
        }

        if (ESTADO_SIGUIENTE == ESTADO_DESCONOCIDO)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_DESCONOCIDO();
        }
    }
}

int set_timer(void)
{
    ESP_LOGI(tag, "Timer init configuration");
    xTimers = xTimerCreate("Timer",              // Just a text name, not used by the kernel.
                           (pdMS_TO_TICKS(50)), // The timer period in ticks.
                           pdTRUE,               // The timer will auto-reload themselves when they expire.
                           (void *)x,            // Assign each timer a unique id equal to its array index.
                           vTimerCallback        // Each timer calls the same callback when it expires.
    );

    if (xTimers == NULL)
    {
        // The timer was not created.
        ESP_LOGE(tag, "The timer was not created.");
    }
    else
    {

        if (xTimerStart(xTimers, 0) != pdPASS)
        {
            // The timer could not be set into the Active state.
            ESP_LOGE(tag, "TThe timer could not be set into the Active state.");
        }
    }

    return 0;
}

int Func_SETTING_IO(void)
{
/*pin_lsc 2 // limit switch puerta cerrada
pin_lsa 4  // limit switch puerta abierta
pin_ftc 5 // fotocelda
pin_keya 18 
pin_keyc 19
pin_pp 21
pin_dpsw_DESCONOCIDO[0] 27// dpsw de dos pines para funcionamiento a partir de desconocido y boton pp desde estado desconocido
pin_dpsw_DESCONOCIDO[1] 14 
pin_dpsw_DETENIDA[0] 12
pin_dpsw_DETENIDA[1]13
pin_ma 26  // motor abrir
pin_mc 25   // motor cerrar
pin_lamp 32 // lampara
pin_buzzer 34*/

gpio_reset_pin(pin_lsc);
gpio_reset_pin(pin_lsa);
gpio_reset_pin(pin_ftc);
gpio_reset_pin(pin_keya);
gpio_reset_pin(pin_keyc);
gpio_reset_pin(pin_pp);
gpio_reset_pin(pin_dpsw_DESCONOCIDO_LSB);
gpio_reset_pin(pin_dpsw_DESCONOCIDO_MSB);
gpio_reset_pin(pin_dpsw_DETENIDA_LSB);
gpio_reset_pin(pin_dpsw_DETENIDA_MSB);

gpio_reset_pin(pin_ma);
gpio_reset_pin(pin_mc);
gpio_reset_pin(pin_lamp);
gpio_reset_pin(pin_buzzer);

gpio_set_direction(pin_lsa, GPIO_MODE_INPUT);
gpio_set_direction(pin_lsc, GPIO_MODE_INPUT);
gpio_set_direction(pin_ftc, GPIO_MODE_INPUT);
gpio_set_direction(pin_keya, GPIO_MODE_INPUT);
gpio_set_direction(pin_keyc, GPIO_MODE_INPUT);
gpio_set_direction(pin_pp, GPIO_MODE_INPUT);
gpio_set_direction(pin_dpsw_DESCONOCIDO_LSB, GPIO_MODE_INPUT);
gpio_set_direction(pin_dpsw_DESCONOCIDO_MSB, GPIO_MODE_INPUT);
gpio_set_direction(pin_dpsw_DETENIDA_LSB, GPIO_MODE_INPUT);
gpio_set_direction(pin_dpsw_DETENIDA_MSB, GPIO_MODE_INPUT);

gpio_set_direction(pin_ma, GPIO_MODE_OUTPUT);
gpio_set_direction(pin_mc, GPIO_MODE_OUTPUT);
gpio_set_direction(pin_lamp, GPIO_MODE_OUTPUT);
gpio_set_direction(pin_buzzer, GPIO_MODE_OUTPUT);

ESP_LOGI(tag, "Entradas asignadas");
return 0;
}

int Func_ESTADO_INICIAL(void)
{
    // establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_INICIAL;

    // inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    // ciclo de estado
    for (;;)
    {
        // detectar error en limit switch
        if (io.lsc == LM_ACTIVO && io.lsa == LM_ACTIVO)
        {
            return ESTADO_ERROR;
        }

        if (io.lsc == LM_INACTIVO && io.lsa == LM_ACTIVO)
        {
            return ESTADO_ABIERTA;
        }

        if (io.lsc == LM_ACTIVO && io.lsa == LM_INACTIVO)
        {
            return ESTADO_CERRADA;
        }

        if (io.lsc == LM_INACTIVO && io.lsa == LM_INACTIVO)
        {
            return ESTADO_DESCONOCIDO;
        }
    }
}
int Func_ESTADO_ERROR(void)
{
    // establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_ERROR;

    // inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    // ciclo de estado
    for (;;)
    {

        io.buzzer = BUZZER_ON;

        // ir a ESTADO_ABIERTA si las condiciones de error han sido corregidas Y esto es lo que indican los limit switchs
        if (io.lsc == LM_INACTIVO && io.lsa == LM_ACTIVO)
        {
            return ESTADO_ABIERTA;
        }

        // ir a ESTADO_CERRADA si las condiciones de error han sido corregidas Y esto es lo que indican los limit switchs
        if (io.lsc == LM_ACTIVO && io.lsa == LM_INACTIVO)
        {
            return ESTADO_CERRADA;
        }

        // ir a ESTADO_CERRANDO si las condiciones de error han sido corregidas
        if (io.lsc == LM_INACTIVO && io.lsa == LM_INACTIVO && io.ftc == FTC_INACTIVO)
        {
            return ESTADO_CERRANDO;
        }
    }
}
int Func_ESTADO_ABRIENDO(void)
{
    // establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_ABRIENDO;

    // inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    // ciclo de estado
    for (;;)
    {
        // se enciende el motor en el sentido que se amerita para que la puerta se ABRA
        io.ma = MOTOR_ON;

        // si se presiona pp mientra la puerta se está ABRIENDO esta se detiene
        if (io.pp == PP_ACTIVO)
        {
            return ESTADO_DETENIDA;
        }

        // lsa activo indica que la puerta se encuentra ABIERTA
        if (io.lsa == LM_ACTIVO)
        {
            return ESTADO_ABIERTA;
        }
    }
}
int Func_ESTADO_CERRANDO(void)
{
    // establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_CERRANDO;

    // inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    // ciclo de estado
    for (;;)
    {
        // se enciende el motor en el sentido que se amerita para que la puerta se CIERRE
        io.mc = MOTOR_ON;

        // si se presiona pp mientra la puerta se está CERRANDO esta se detiene
        if (io.pp == PP_ACTIVO)
        {
            return ESTADO_DETENIDA;
        }

        // lsc activo indica que la puerta se encuentra CERRADA
        if (io.lsc == LM_ACTIVO)
        {
            return ESTADO_CERRADA;
        }
    }
}
int Func_ESTADO_ABIERTA(void)
{
    // establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_ABIERTA;

    // inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    // ciclo de estado
    for (;;)
    {

        // estando la puerta abierta indicar por medio de la llave O presionar pp es causa suficiente para que esta se CIERRE
        if (io.keyc == KEY_ACTIVO || io.pp == PP_ACTIVO)
        {
            return ESTADO_CERRANDO;
        }
    }
}
int Func_ESTADO_CERRADA(void)
{
    // establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_CERRADA;

    // inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    // ciclo de estado
    for (;;)
    {

        // estando la puerta cerrada indicar por medio de la llave O presionar pp es causa suficiente para que esta se ABRA
        if (io.keya == KEY_ACTIVO || io.pp == PP_ACTIVO)
        {
            return ESTADO_CERRANDO;
        }
    }
}
int Func_ESTADO_DETENIDA(void)
{
    // establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_DETENIDA;

    // inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    // ciclo de estado
    for (;;)
    {

        // estando la puerta DETENIDA indicar por medio de la llave es causa suficiente para que esta se ABRA O se CIERRE
        if (io.keya == KEY_ACTIVO)
        {
            return ESTADO_ABRIENDO;
        }

        if (io.keyc == KEY_ACTIVO)
        {
            return ESTADO_CERRANDO;
        }

        /*estando la puerta DETENIDA su comportamiento al presionar pp dependera del estado del dpsw_DETENIDA
        00 seguir
        01 contrario*/
        if (io.pp == PP_ACTIVO && config.FDETENIDA == FDETENIDA_SEGUIR)
        {
            return ESTADO_ANTERIOR;
        }

        if (io.pp == PP_ACTIVO && config.FDETENIDA == FDETENIDA_CONTRARIO)
        {
            if (ESTADO_ANTERIOR == ESTADO_ABRIENDO)
            {
                return ESTADO_CERRANDO;
            }

            if (ESTADO_ANTERIOR == ESTADO_CERRANDO)
            {
                return ESTADO_ABRIENDO;
            }
        }
    }
}
int Func_ESTADO_DESCONOCIDO(void)
{
    // establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_DESCONOCIDO;

    // inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    // ciclo de estado
    for (;;)
    {
        /*El comportamiento de la puerta estado en el estado desconocido
        depende del estado del dpsw_DESCONOCIDO de modo que:
        00 cerrar
        01 abrir
        10 esperar
        11 parpadear*/

        if (config.FDESCONOCIDO == FDESCONOCIDO_CIERRA)
        {
            return ESTADO_CERRANDO;
        }

        if (config.FDESCONOCIDO == FDESCONOCIDO_ABRIR)
        {
            return ESTADO_ABRIENDO;
        }

        if (config.FDESCONOCIDO == FDESCONOCIDO_ESPERA)
        {
            if (io.keya == KEY_ACTIVO)
            {
                return ESTADO_ABRIENDO;
            }

            if (io.keyc == KEY_ACTIVO)
            {
                return ESTADO_CERRANDO;
            }
        }

        if (config.FDESCONOCIDO == FDESCONOCIDO_PARPADEAR)
        {
            if (io.pp == PP_ACTIVO)
            {
                return ESTADO_CERRANDO;
            }
            // CODIGO PARA HACER PARPADEAR LA LAMPARA
        }
    }
}
