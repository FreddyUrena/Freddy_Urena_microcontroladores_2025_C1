/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include "esp_netif.h"
#include <esp_http_server.h>
#include "esp_tls.h"
#include "esp_check.h"
#include <time.h>
#include <sys/time.h>
#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#if !CONFIG_IDF_TARGET_LINUX
#endif // !CONFIG_IDF_TARGET_LINUX

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN (64)

static const char *TAG = "WebServer";

#define LED 25
#define PB_PIN 18
unsigned int BOTON = 0;

#define BOTON_ACTIVADO 0
#define BOTON_DESACTIVADO 1

float rm_value = 0.0;
float cm_value = 0.0;
float r1a_value = 0.0;
float r2a_value = 0.0;
float ca_value = 0.0;
float dutycycle_value = 0.0;
float DCToSet = 0.0;
char mode_value[32] = {0};

float TM = 0.0;
float THA = 0.0;
float TLA = 0.0;

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO (33) // Define the output GPIO
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY (4096)                // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY (4000)           // Frequency in Hertz. Set frequency at 4 kHz

static void ledc_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY, // Set output frequency at 4 kHz
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = LEDC_OUTPUT_IO,
        .duty = 0, // Set duty to 0%
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

// void func_modo_monoestable(void);

/*********************************WEB SERVER CODE BEGINS*******************************/

/* An HTTP GET handler */
static esp_err_t home_get_handler(httpd_req_t *req)
{
    esp_err_t error;
    ESP_LOGI(TAG, "LED Turned OFF");
    const char *response = (const char *)req->user_ctx;
    error = httpd_resp_send(req, response, strlen(response));
    if (error != ESP_OK)
    {
        ESP_LOGI(TAG, "Error %d while sending Response", error);
    }
    else
        ESP_LOGI(TAG, "Response sent Successfully");

    return error;
}

static const httpd_uri_t home = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = home_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = "<!DOCTYPE html>"
                "<html>"
                "<head>"
                "  <meta charset='UTF-8'>"
                "  <title>Simulador NE555</title>"
                "  <style>"
                "    body {"
                "      font-family: sans-serif;"
                "      text-align: center;"
                "      padding: 20px;"
                "    }"
                "    select, input {"
                "      margin: 10px;"
                "      padding: 5px;"
                "      width: 200px;"
                "    }"
                "    .params {"
                "      display: none;"
                "    }"
                "    .slider-value {"
                "      font-weight: bold;"
                "    }"
                "  </style>"
                "</head>"
                "<body>"
                "  <h1>Simulador NE555</h1>"
                "  <form id='configForm'>"
                "    <label for='mode'>Modo:</label>"
                "    <select id='mode' name='mode' onchange='updateFields()'>"
                "      <option value='monoestable'>Monoestable</option>"
                "      <option value='astable'>Astable</option>"
                "      <option value='pwm'>PWM</option>"
                "    </select>"
                "    <!-- MONOESTABLE -->"
                "    <div id='monoestableFields' class='params'>"
                "      <input type='number' name='resistor' placeholder='Resistencia R (Ω)' required>"
                "      <input type='number' name='capacitor' placeholder='Capacitor C (μF)' required>"
                "    </div>"
                "    <!-- ASTABLE -->"
                "    <div id='astableFields' class='params'>"
                "      <input type='number' name='r1' placeholder='Resistencia R1 (Ω)' required>"
                "      <input type='number' name='r2' placeholder='Resistencia R2 (Ω)' required>"
                "      <input type='number' name='c' placeholder='Capacitor C (μF)' required>"
                "    </div>"
                "    <!-- PWM -->"
                "    <div id='pwmFields' class='params'>"
                "      <label for='duty'>Duty Cycle: <span id='dutyValue' class='slider-value'>50</span>%</label><br>"
                "      <input type='range' id='duty' name='duty' min='0' max='100' value='50' oninput='updateSliderValue()'>"
                "    </div>"
                "    <br>"
                "    <button type='submit'>Enviar</button>"
                "  </form>"
                "  <script>"
                "    function updateFields() {"
                "      const mode = document.getElementById('mode').value;"
                "      document.querySelectorAll('.params').forEach(div => div.style.display = 'none');"
                "      document.getElementById(mode + 'Fields').style.display = 'block';"
                "    }"
                "    function updateSliderValue() {"
                "      const val = document.getElementById('duty').value;"
                "      document.getElementById('dutyValue').innerText = val;"
                "    }"
                "    document.getElementById('configForm').addEventListener('submit', function(e) {"
                "      e.preventDefault();"
                "      const mode = document.getElementById('mode').value;"
                "      const formData = new FormData(this);"
                "      const data = { mode };"
                "      formData.forEach((value, key) => {"
                "        if (key !== 'mode') data[key] = parseFloat(value);"
                "      });"
                "      fetch('/configuracion', {"
                "        method: 'POST',"
                "        headers: {'Content-Type': 'application/json'},"
                "        body: JSON.stringify(data)"
                "      }).then(res => {"
                "        if (res.ok) alert('Configuración enviada correctamente');"
                "        else alert('Error al enviar configuración');"
                "      });"
                "    });"
                "    // Inicializar campos visibles"
                "    updateFields();"
                "  </script>"
                "</body>"
                "</html>"};

/*POST PARA RECIBIR DATA*/
static esp_err_t configuracion_post_handler(httpd_req_t *req)
{
    char content[100];
    int ret, remaining = req->content_len;

    // Leemos el contenido del cuerpo de la solicitud POST
    while (remaining > 0)
    {
        ret = httpd_req_recv(req, content, MIN(remaining, sizeof(content)));
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }

    // Procesar datos JSON
    cJSON *json = cJSON_Parse(content);
    if (json == NULL)
    {
        ESP_LOGE(TAG, "Error parsing JSON");
        return ESP_FAIL;
    }

    /*float rm_value = 0.0;
    float cm_value = 0.0;
    float r1a_value = 0.0;
    float r2a_value = 0.0;
    float ca_value = 0.0;
    float dutycycle_value = 0.0;
    char mode_value[32] = {0};*/

    // Obtener los valores del JSON:
    const cJSON *mode = cJSON_GetObjectItem(json, "mode");

    if (mode != NULL)
    {
        ESP_LOGI(TAG, "Modo seleccionado: %s", mode->valuestring);
        strncpy(mode_value, mode->valuestring, sizeof(mode_value) - 1);
    }

    const cJSON *r_m = cJSON_GetObjectItem(json, "resistor");
    if (r_m == NULL)
    {
        ESP_LOGE(TAG, "No se encontró la clave 'resistor' en el JSON");
    }
    else
    {
        rm_value = r_m->valuedouble;
        ESP_LOGI(TAG, "Resistor value: %f", r_m->valuedouble);
    }

    const cJSON *c_m = cJSON_GetObjectItem(json, "capacitor");
    if (c_m == NULL)
    {
        ESP_LOGE(TAG, "No se encontró la clave 'capacitor' en el JSON");
    }
    else
    {
        cm_value = c_m->valuedouble;
        ESP_LOGI(TAG, "Capacitor value: %f", c_m->valuedouble);
    }

    const cJSON *r1_a = cJSON_GetObjectItem(json, "r1");
    if (r1_a == NULL)
    {
        ESP_LOGE(TAG, "No se encontró la clave 'r1' en el JSON");
    }
    else
    {
        ESP_LOGI(TAG, "R1_A value: %f", r1_a->valuedouble);
        r1a_value = r1_a->valuedouble;
    }

    const cJSON *r2_a = cJSON_GetObjectItem(json, "r2");
    if (r2_a == NULL)
    {
        ESP_LOGE(TAG, "No se encontró la clave 'r2' en el JSON");
    }
    else
    {
        ESP_LOGI(TAG, "R2_A value: %f", r2_a->valuedouble);
        r2a_value = r2_a->valuedouble;
    }

    const cJSON *c_a = cJSON_GetObjectItem(json, "c");
    if (c_a == NULL)
    {
        ESP_LOGE(TAG, "No se encontró la clave 'c' en el JSON");
    }
    else
    {
        ESP_LOGI(TAG, "C_a value: %f", c_a->valuedouble);
        ca_value = c_a->valuedouble;
    }

    const cJSON *dut = cJSON_GetObjectItem(json, "duty");
    if (dut == NULL)
    {
        ESP_LOGE(TAG, "No se encontró la clave 'duty' en el JSON");
    }
    else
    {
        ESP_LOGI(TAG, "Duty cycle: %f", dut->valuedouble);
        dutycycle_value = dut->valuedouble;
    }

    const char *response = "Configuración recibida";

    if (mode != NULL)
    {

        // Responder con éxito
        httpd_resp_send(req, response, strlen(response));

        // ESP_LOGI(TAG, "Modo seleccionado: %s", mode->valuestring);
        /*if (strcmp(mode->valuestring, "monoestable") == 0)
         {
             // ESP_LOGI(TAG, "Modo monoestable");

            // TM = 1100 * rm_value * cm_value;

             ESP_LOGI(TAG, "Pulso de salida monoestable en mS: %f", TM);

             // func_modo_monoestable();
         }
         else if (strcmp(mode->valuestring, "astable") == 0)
         {
             // ESP_LOGI(TAG, "Modo astable");
         }
         else if (strcmp(mode->valuestring, "pwm") == 0)
         {
             // ESP_LOGI(TAG, "Modo PWM");
         }*/
    }

    cJSON_Delete(json);

    return ESP_OK;
}

static const httpd_uri_t configuracion = {
    .uri = "/configuracion",
    .method = HTTP_POST,
    .handler = configuracion_post_handler,
    .user_ctx = NULL,
};
/*POST PARA RECIBIR DATA*/

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
#if CONFIG_IDF_TARGET_LINUX
    // Setting port as 8001 when building for Linux. Port 80 can be used only by a privileged user in linux.
    // So when a unprivileged user tries to run the application, it throws bind error and the server is not started.
    // Port 8001 can be used by an unprivileged user as well. So the application will not throw bind error and the
    // server will be started.
    config.server_port = 8001;
#endif // !CONFIG_IDF_TARGET_LINUX
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &home);
        httpd_register_uri_handler(server, &configuracion);
        /*httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &ctrl);
        httpd_register_uri_handler(server, &any);*/

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

#if !CONFIG_IDF_TARGET_LINUX
static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server)
    {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK)
        {
            *server = NULL;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL)
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}
#endif

// !CONFIG_IDF_TARGET_LINUX

/**************************WEB SERVER CODE ENDS*****************************/

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                .required = true,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

int set_timer(void);

TimerHandle_t xTimers;
int x;

void vTimerCallback(TimerHandle_t pxTimer)
{
    if (gpio_get_level(PB_PIN))
    {
        BOTON = BOTON_DESACTIVADO;
    }
    else
    {
        BOTON = BOTON_ACTIVADO;
    }
}

unsigned int RETENCION = 0;

/*void func_modo_monoestable(void)
{
        gpio_set_level(LED, 1);
        vTaskDelay(TM / portTICK_PERIOD_MS);
        gpio_set_level(LED, 0);
}*/

void app_main(void)
{

    gpio_reset_pin(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);

    gpio_reset_pin(PB_PIN);
    gpio_set_direction(PB_PIN, GPIO_MODE_INPUT);

    static httpd_handle_t server = NULL;

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &connect_handler, &server));
    // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

    set_timer();

    // Set the LEDC peripheral configuration
    ledc_init();
    // Set duty to 50%
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

    while (1)
    {
        if (mode_value[0] != '\0')
        {
            // ESP_LOGI(TAG, "Modo seleccionado: %s", mode->valuestring);
            if (strcmp(mode_value, "monoestable") == 0)
            {
                // ESP_LOGI(TAG, "Modo monoestable");

                TM = 1100 * rm_value * cm_value;

                if (BOTON == BOTON_ACTIVADO)
                { /*gpio_set_level(LED, 1);
                      || RETENCION == 1
                     RETENCION = 1;*/
                    gpio_set_level(LED, 1);
                    vTaskDelay(TM / portTICK_PERIOD_MS);
                    gpio_set_level(LED, 0);
                    // RETENCION = 0;
                }

                else
                {
                    gpio_set_level(LED, 0);
                }

                /* ESP_LOGI(TAG, "Pulso de salida monoestable en mS %f", TM);

                 gpio_set_level(LED, 1);
                 vTaskDelay(rm_value/portTICK_PERIOD_MS);
                 gpio_set_level(LED, 0);
                 ESP_LOGI(TAG, "LED APAGADO");*/
            }
            else if (strcmp(mode_value, "astable") == 0)
            {
                // ESP_LOGI(TAG, "Modo astable");
                /*r1a_value = 0.0;
                r2a_value = 0.0;
                ca_value = 0.0;*/

                THA = 693 * (r1a_value + r2a_value) * ca_value;
                TLA = 693 * r2a_value * ca_value;

                gpio_set_level(LED, 1);
                vTaskDelay(THA / portTICK_PERIOD_MS);
                gpio_set_level(LED, 0);
                vTaskDelay(TLA / portTICK_PERIOD_MS);
            }
            else if (strcmp(mode_value, "pwm") == 0)
            {
                DCToSet = dutycycle_value*81;
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, DCToSet);
                // Update duty to apply the new value
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

int set_timer(void)
{
    ESP_LOGI(TAG, "Timer init configuration");
    xTimers = xTimerCreate("Timer",             // Just a text name, not used by the kernel.
                           (pdMS_TO_TICKS(50)), // The timer period in ticks.
                           pdTRUE,              // The timer will auto-reload themselves when they expire.
                           (void *)x,           // Assign each timer a unique id equal to its array index.
                           vTimerCallback       // Each timer calls the same callback when it expires.
    );

    if (xTimers == NULL)
    {
        // The timer was not created.
        ESP_LOGE(TAG, "The timer was not created.");
    }
    else
    {

        if (xTimerStart(xTimers, 0) != pdPASS)
        {
            // The timer could not be set into the Active state.
            ESP_LOGE(TAG, "The timer could not be set into the Active state.");
        }
    }

    return 0;
}