#include <stdio.h>
#include <stdlib.h>

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

int Func_ESTADO_INICIAL(void);
int Func_ESTADO_ERROR(void);
int Func_ESTADO_ABRIENDO(void);
int Func_ESTADO_CERRANDO(void);
int Func_ESTADO_ABIERTA(void);
int Func_ESTADO_CERRADA(void);
int Func_ESTADO_DETENIDA(void);
int Func_ESTADO_DESCONOCIDO(void);

/*typedef enum
{
    ESTADO_INICIAL,
    ESTADO_ERROR,
    ESTADO_ABRIENDO,
    ESTADO_CERRANDO,
    ESTADO_ABIERTA,
    ESTADO_CERRADA,
    ESTADO_DETENIDA,
    ESTADO_DESCONICIDO
} estado;*/

int ESTADO_SIGUIENTE = ESTADO_INICIAL;
int ESTADO_ANTERIOR = ESTADO_INICIAL;
int ESTADO_ACTUAL = ESTADO_INICIAL;
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
    unsigned int lsc:1; //limit switch puerta cerrada
    unsigned int lsa:1; //limit switch puerta abierta
    unsigned int ftc:1; //fotocelda
    unsigned int ma:1; //motor abrir
    unsigned int mc:1; //motor cerrar
    unsigned int lamp:1; //lampara
    unsigned int buzzer:1;
    unsigned int keya:1;
    unsigned int keyc:1;
    unsigned int pp:1;
    unsigned int dpsw_DESCONOCIDO:2; //dpsw de dos pines para funcionamiento a partir de desconocido y boton pp desde estado desconocido
    unsigned int dpsw_DETENIDA:2;
} io;

struct SYSTEM_CONFIG
{
    unsigned int cnt_TA;//contador en segundos del tiempo de cierre automatico
    unsigned int cnt_Tc;//contador en segundos del tiempo de cierre automatico
    unsigned int cnt_RT;//contador en segundos del tiempo maximo de movimiento del motor
    int FDESCONOCIDO;
    int FDETENIDA;//contiene la configuracion del modo detenido
} config;

int main()
{
    for(;;)
    {
        if(ESTADO_SIGUIENTE == ESTADO_INICIAL)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_INICIAL();
        }

        if(ESTADO_SIGUIENTE == ESTADO_ERROR)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_ERROR();
        }

        if(ESTADO_SIGUIENTE == ESTADO_ABRIENDO)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_ABRIENDO();
        }

        if(ESTADO_SIGUIENTE == ESTADO_CERRANDO)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_CERRANDO();
        }

        if(ESTADO_SIGUIENTE == ESTADO_ABIERTA)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_ABIERTA();
        }

        if(ESTADO_SIGUIENTE == ESTADO_CERRADA)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_CERRADA();
        }

        if(ESTADO_SIGUIENTE == ESTADO_DETENIDA)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_DETENIDA();
        }

        if(ESTADO_SIGUIENTE == ESTADO_DESCONOCIDO)
        {
            ESTADO_SIGUIENTE = Func_ESTADO_DESCONOCIDO();
        }
    }

    return 0;
}

int Func_ESTADO_INICIAL(void)
{
    //establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_INICIAL;

    //inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    //ciclo de estado
    for(;;)
    {
        //detectar error en limit switch
        if(io.lsc == LM_ACTIVO && io.lsa == LM_ACTIVO)
        {
            return ESTADO_ERROR;
        }

        if(io.lsc == LM_INACTIVO && io.lsa == LM_ACTIVO)
        {
            return ESTADO_ABIERTA;
        }

        if(io.lsc == LM_ACTIVO && io.lsa == LM_INACTIVO)
        {
            return ESTADO_CERRADA;
        }

        if(io.lsc == LM_INACTIVO && io.lsa == LM_INACTIVO)
        {
            return ESTADO_DESCONOCIDO;
        }

    }
}
int Func_ESTADO_ERROR(void)
{
    //establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_ERROR;

    //inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    //ciclo de estado
    for(;;)
    {

        io.buzzer = BUZZER_ON;

        //ir a ESTADO_ABIERTA si las condiciones de error han sido corregidas Y esto es lo que indican los limit switchs
        if(io.lsc == LM_INACTIVO && io.lsa == LM_ACTIVO)
        {
            return ESTADO_ABIERTA;
        }

        //ir a ESTADO_CERRADA si las condiciones de error han sido corregidas Y esto es lo que indican los limit switchs
        if(io.lsc == LM_ACTIVO && io.lsa == LM_INACTIVO)
        {
            return ESTADO_CERRADA;
        }

        //ir a ESTADO_CERRANDO si las condiciones de error han sido corregidas
        if(io.lsc == LM_INACTIVO && io.lsa == LM_INACTIVO && io.ftc == FTC_INACTIVO)
        {
            return ESTADO_CERRANDO;
        }
    }
}
int Func_ESTADO_ABRIENDO(void)
{
    //establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_ABRIENDO;

    //inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    //ciclo de estado
    for(;;)
    {
        //se enciende el motor en el sentido que se amerita para que la puerta se ABRA
        io.ma = MOTOR_ON;

        //si se presiona pp mientra la puerta se está ABRIENDO esta se detiene
        if(io.pp == PP_ACTIVO)
        {
            return ESTADO_DETENIDA;
        }

        //lsa activo indica que la puerta se encuentra ABIERTA
        if(io.lsa == LM_ACTIVO)
        {
            return ESTADO_ABIERTA;
        }

    }
}
int Func_ESTADO_CERRANDO(void)
{
    //establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_CERRANDO;

    //inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    //ciclo de estado
    for(;;)
    {
        //se enciende el motor en el sentido que se amerita para que la puerta se CIERRE
        io.mc = MOTOR_ON;

        //si se presiona pp mientra la puerta se está CERRANDO esta se detiene
        if(io.pp == PP_ACTIVO)
        {
            return ESTADO_DETENIDA;
        }

        //lsc activo indica que la puerta se encuentra CERRADA
        if(io.lsc == LM_ACTIVO)
        {
            return ESTADO_CERRADA;
        }
    }
}
int Func_ESTADO_ABIERTA(void)
{
    //establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_ABIERTA;

    //inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    //ciclo de estado
    for(;;)
    {

        //estando la puerta abierta indicar por medio de la llave O presionar pp es causa suficiente para que esta se CIERRE
        if(io.keyc == KEY_ACTIVO || io.pp == PP_ACTIVO)
        {
            return ESTADO_CERRANDO;
        }

    }
}
int Func_ESTADO_CERRADA(void)
{
    //establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_CERRADA;

    //inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    //ciclo de estado
    for(;;)
    {

        //estando la puerta cerrada indicar por medio de la llave O presionar pp es causa suficiente para que esta se ABRA
        if(io.keya == KEY_ACTIVO || io.pp == PP_ACTIVO)
        {
            return ESTADO_CERRANDO;
        }

    }
}
int Func_ESTADO_DETENIDA(void)
{
    //establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_DETENIDA;

    //inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    //ciclo de estado
    for(;;)
    {

        //estando la puerta DETENIDA indicar por medio de la llave es causa suficiente para que esta se ABRA O se CIERRE
        if(io.keya == KEY_ACTIVO)
        {
            return ESTADO_ABRIENDO;
        }

        if(io.keyc == KEY_ACTIVO)
        {
            return ESTADO_CERRANDO;
        }

        /*estando la puerta DETENIDA su comportamiento al presionar pp dependera del estado del dpsw_DETENIDA
        00 seguir
        01 contrario*/
        if(io.pp == PP_ACTIVO && config.FDETENIDA == FDETENIDA_SEGUIR)
        {
            return ESTADO_ANTERIOR;
        }

        if(io.pp == PP_ACTIVO && config.FDETENIDA == FDETENIDA_CONTRARIO)
        {
            if(ESTADO_ANTERIOR == ESTADO_ABRIENDO)
            {
                return ESTADO_CERRANDO;
            }

            if(ESTADO_ANTERIOR == ESTADO_CERRANDO)
            {
                return ESTADO_ABRIENDO;
            }
        }

    }
}
int Func_ESTADO_DESCONOCIDO(void)
{
    //establecer los estados anterior y actual
    ESTADO_ANTERIOR = ESTADO_ACTUAL;
    ESTADO_ACTUAL = ESTADO_DESCONOCIDO;

    //inicializar el estado
    io.ma = MOTOR_OFF;
    io.mc = MOTOR_OFF;
    io.lamp = LAMP_OFF;
    io.buzzer = BUZZER_OFF;

    //ciclo de estado
    for(;;)
    {
        /*El comportamiento de la puerta estado en el estado desconocido
        depende del estado del dpsw_DESCONOCIDO de modo que:
        00 cerrar
        01 abrir
        10 esperar
        11 parpadear*/

        if(config.FDESCONOCIDO == FDESCONOCIDO_CIERRA)
        {
            return ESTADO_CERRANDO;
        }

        if(config.FDESCONOCIDO == FDESCONOCIDO_ABRIR)
        {
            return ESTADO_ABRIENDO;
        }

        if(config.FDESCONOCIDO == FDESCONOCIDO_ESPERA)
        {
            if(io.keya == KEY_ACTIVO)
            {
                return ESTADO_ABRIENDO;
            }

            if(io.keyc == KEY_ACTIVO)
            {
                return ESTADO_CERRANDO;
            }
        }

        if(config.FDESCONOCIDO == FDESCONOCIDO_PARPADEAR)
        {
            if(io.pp == PP_ACTIVO)
            {
                return ESTADO_CERRANDO;
            }
            //CODIGO PARA HACER PARPADEAR LA LAMPARA

        }
    }
}
