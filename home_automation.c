#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"

#define WIFI_SSID "Willian(gdv)18am"  // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASS "c4iqu3246" // Substitua pela senha da sua rede Wi-Fi

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15 
#define address 0x3C

#define WS2812_PIN 7
#define BUTTON_B_PIN 6

#define SERVO_PIN 8  // Definição do pino PWM para o servo
#define PWM_FREQ 50   // Frequência de 50Hz (Período de 20ms)
#define PWM_WRAP 20000 // Contagem total do PWM (20ms em microsegundos

#define CHECKED(thing) (thing ? "checked" : "")

ssd1306_t ssd;

/*
    * Variável para armazenar o estado dos dispositivos
    * do sistema de automação residencial.
*/
typedef struct {
    bool gate;
    bool living_room_light;
    bool kitchen_light;
    bool bedroom_light;
} THINGS;

enum THINGS_MATRIX_5X5_POSITION {
    LIVING_ROOM_LIGHT = 14,
    KITCHEN_LIGHT = 12,
    BEDROOM_LIGHT = 10,
};

THINGS things = {false, false, false, false};

typedef struct {
    uint8_t temperature;
    uint8_t humidity;
} DHT11;

DHT11 dht11 = {0, 0};

/*
    * Variável para armazenar a resposta HTTP
    * O tamanho máximo da resposta é 3072 bytes
*/
char http_response[3072];

void gpio_setup();
void i2c_setup();
void show_ip_address();
void gpio_irq_handler(uint gpio, uint32_t events);
void create_http_response();
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err);
static void start_http_server(void);
void npInit(uint pin);
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b);
void npClear();
void npWrite();
void turn_on_light(bool *light, uint index);
void turn_off_light(bool *light, uint index);
void gate(bool state);
void emit_alert();
void setup_pwm(uint pin);
void set_servo_position(uint pin, uint pulse_width);
int read_dht11(DHT11 *sensor);

#define LED_COUNT 25

struct pixel_t {
  uint8_t R, G, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

PIO np_pio;
uint sm;
npLED_t leds[LED_COUNT];


int main()
{
    stdio_init_all();
    gpio_setup();
    i2c_setup();

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, address, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return -1;
    }

    printf("Conectado.\n");
    
    read_dht11(&dht11);  // Lê os dados do sensor DHT11
    show_ip_address();

    start_http_server();


    while (true) {
        cyw43_arch_poll();  // Necessário para manter o Wi-Fi ativo
        sleep_ms(100);
    }

    cyw43_arch_deinit();  // Desliga o Wi-Fi
    return 0;
}

void gpio_setup() {
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);
    gpio_set_irq_enabled_with_callback(
        BUTTON_B_PIN, 
        GPIO_IRQ_EDGE_FALL,
        true, 
        &gpio_irq_handler
    );
}

void i2c_setup() {
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

void show_ip_address() {
    uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
    printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

    char ip_str_0[4];
    char ip_str_1[4];
    char ip_str_2[4];
    char ip_str_3[4];
    char temp_str[6];
    char hum_str[6];
    sprintf(temp_str, "%d C", dht11.temperature);
    sprintf(hum_str, "%d%%", dht11.humidity);
    sprintf(ip_str_0, "%d.", ip_address[0]);
    sprintf(ip_str_1, "%d.", ip_address[1]);
    sprintf(ip_str_2, "%d.", ip_address[2]);
    sprintf(ip_str_3, "%d", ip_address[3]);

    ssd1306_draw_string(&ssd, "Temp: ", 3, 3);
    ssd1306_draw_string(&ssd, temp_str, 56, 3);
    ssd1306_draw_string(&ssd, "Umidade: ", 3, 13);
    ssd1306_draw_string(&ssd, hum_str, 80, 13);

    ssd1306_draw_string(&ssd, ip_str_0, 1, 30);
    ssd1306_draw_string(&ssd, ip_str_1, 26, 30);
    ssd1306_draw_string(&ssd, ip_str_2, 42, 30);
    ssd1306_draw_string(&ssd, ip_str_3, 59, 30);
    ssd1306_send_data(&ssd);
}

/**
 * @brief Manipulador de interrupção para GPIO.
 *
 * Esta função lida com os eventos de interrupção gerados pelos pinos GPIO,
 * realizando as ações necessárias com base no tipo de evento ocorrido.
 *
 * @param gpio Número do GPIO que acionou a interrupção.
 * @param events Máscara de eventos indicando os tipos de interrupção ocorridos.
 */
void gpio_irq_handler(uint gpio, uint32_t events)
{
    volatile static uint32_t last_time = 0;
    volatile uint32_t current_time = to_ms_since_boot(get_absolute_time()); 

    if (current_time - last_time < 400) { // Debounce de 400ms
        return;
    }

    last_time = current_time;

    if (gpio == BUTTON_B_PIN) {
        reset_usb_boot(0, 0);
        return;
    }
}


/**
 * create_http_response - Gera uma resposta HTTP para a automação residencial.
 *
 * Esta função constrói a resposta HTTP preparando os cabeçalhos necessários
 * e o conteúdo do corpo para a comunicação com os dispositivos clientes.
 * Destina-se a ser parte do tratamento de respostas do sistema de automação residencial.
 *
 * Nota: Os detalhes da implementação dependerão dos requisitos específicos
 * do formato da resposta HTTP e podem ser estendidos conforme necessário.
 */
void create_http_response() {
    snprintf(
        http_response, 
        sizeof(http_response),
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n"
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta charset=\"UTF-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">"
        "<title>AutoHome | Automação residencial</title>"
        "<style>body{font-family:sans-serif;background-color:#f4f7f6;margin:0;padding:1rem;color:#555;min-height:100vh;display:flex;justify-content:center;align-items:center}h2{text-align:center;color:#333}main{width: 100%%;max-width:400px;margin:0 auto;background-color:#fff;padding:1rem;border-radius:10px;box-shadow:0 4px 6px rgba(0,0,0,0.1);display:flex;flex-direction:column;gap:1.5rem}fieldset{border:2px solid #5a5d7c;border-radius:8px;padding:1rem}legend{padding:0 0.5rem;font-weight:bold;color:#000}label{font-weight:bold;margin-right:0.5rem}input[type=\"radio\"]{margin-right:0.5rem}input[type=\"button\"]{background-color:#cf0606;color:#fff;border:none;padding:0.5rem 1rem;border-radius:5px;cursor:pointer}fieldset>div{display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center}.on_status{color:#12db55}.on_status input[type=\"radio\"]{accent-color:#12db55}.off_status{color:#db1212}.off_status input[type=\"radio\"]{accent-color:#db1212}.clima{background-color:#e0f7fa}.clima p{margin:0;line-height:1.5;display:flex;justify-content:space-between}input[type=\"button\"]{text-transform: capitalize}</style>"
        "</head>"
        "<main>"
        "<h2>AutoHome</h2>"
        "<fieldset class=\"clima\">"
        "<legend>Clima</legend>"
        "<p>Temperatura: <strong>%d° C</strong></p>"
        "<p>Umidade: <strong>%d%%</strong></p>"
        "</fieldset>"
        "<fieldset>"
        "<legend>Portão</legend>"
        "<label class=\"on_status\">"
        "<input type=\"radio\" name=\"portao\" value=\"p_o\" %s>Abrir"
        "</label>"
        "<label class=\"off_status\">"
        "<input type=\"radio\" name=\"portao\" value=\"p_c\" class=\"off_status\" %s>Fechar"
        "</label>"
        "</fieldset>"
        "<fieldset>"
        "<legend>Luzes</legend>"
        "<div>"
        "<label>Sala:</label>"
        "<div>"
        "<label class=\"on_status\">"
        "<input type=\"radio\" name=\"sala\" value=\"s_on\" class=\"on_status\" %s>ON"
        "</label>"
        "<label class=\"off_status\">"
        "<input type=\"radio\" name=\"sala\" value=\"s_off\" class=\"off_status\" %s>OFF"
        "</label>"
        "</div>"
        "</div>"
        "<div>"
        "<label>Cozinha:</label>"
        "<div>"
        "<label class=\"on_status\">"
        "<input type=\"radio\" name=\"cozinha\" value=\"c_on\" %s>ON"
        "</label>"
        "<label class=\"off_status\">"
        "<input type=\"radio\" name=\"cozinha\" value=\"c_off\" class=\"off_status\" %s>OFF"    
        "</label>"
        "</div>"
        "</div>"
        "<div>"
        "<label>Quarto:</label>"
        "<div>"
        "<label class=\"on_status\">"
        "<input type=\"radio\" name=\"quarto\" value=\"q_on\" %s>ON"
        "</label>"
        "<label class=\"off_status\">"
        "<input type=\"radio\" name=\"quarto\" value=\"q_off\" class=\"off_status\" %s>OFF"
        "</label>"
        "</div>"
        "</div>"
        "</fieldset>"
        "<fieldset>"
        "<legend>Alertas</legend>"
        "<input type=\"button\" value=\"alerta\">"
        "</fieldset>"
        "</main>"
        "<script>"
        "document.querySelectorAll('input').forEach(i=>{i.addEventListener('click',e=>{window.location.href='/'+i.value})})"
        "</script>"
        "</body>"
        "</html>\r\n",
        dht11.temperature,
        dht11.humidity,
        CHECKED(things.gate),
        CHECKED(!things.gate),
        CHECKED(things.living_room_light),
        CHECKED(!things.living_room_light),
        CHECKED(things.kitchen_light),
        CHECKED(!things.kitchen_light),
        CHECKED(things.bedroom_light),
        CHECKED(!things.bedroom_light)
    );
}

static err_t http_sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    tcp_close(tpcb);
    return ERR_OK;
}

/**
 * @brief Função de callback HTTP para processamento de pacotes TCP recebidos.
 *
 * Esta função é invocada pela pilha TCP quando um novo pbuf é recebido para o 
 * bloco de controle do protocolo TCP (PCB) associado à conexão. A função é responsável 
 * por tratar os dados do pacote, processar requisições HTTP e gerenciar quaisquer erros 
 * que possam ocorrer durante a transmissão.
 *
 * @param arg  Ponteiro genérico para passar dados específicos da aplicação.
 * @param tpcb Ponteiro para o bloco de controle do protocolo TCP associado à conexão.
 * @param p    Ponteiro para o buffer (pbuf) que contém os dados do pacote recebido.
 * @param err  Código de erro referente ao processamento do pacote recebido.
 *
 * @return err_t Código de status que indica o resultado da operação de processamento.
 *               Normalmente, retorna ERR_OK se o processamento for bem-sucedido.
 */
static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        // Cliente fechou a conexão
        tcp_close(tpcb);
        return ERR_OK;
    }

    tcp_recved(tpcb, p->len);  // Indica que o pacote foi recebido

    // Processa a requisição HTTP
    char *request = (char *)p->payload;

    if (strstr(request, "GET /p_o")) {
        gate(true);
    } else if (strstr(request, "GET /p_c")) {
        gate(false);
    } else if (strstr(request, "GET /s_on")) {
        turn_on_light(&things.living_room_light, LIVING_ROOM_LIGHT);
        printf("Luz da sala ligada\n");
    } else if (strstr(request, "GET /s_off")) {
        turn_off_light(&things.living_room_light, LIVING_ROOM_LIGHT);
        printf("Luz da sala desligada\n");
    } else if (strstr(request, "GET /c_on")) {
        turn_on_light(&things.kitchen_light, KITCHEN_LIGHT);
        printf("Luz da cozinha ligada\n");
    } else if (strstr(request, "GET /c_off")) {
        turn_off_light(&things.kitchen_light, KITCHEN_LIGHT);
        printf("Luz da cozinha desligada\n");
    } else if (strstr(request, "GET /q_on")) {
        turn_on_light(&things.bedroom_light, BEDROOM_LIGHT);
        printf("Luz do quarto ligada\n");
    } else if (strstr(request, "GET /q_off")) {
        turn_off_light(&things.bedroom_light, BEDROOM_LIGHT);
        printf("Luz do quarto desligada\n");
    } else if (strstr(request, "GET /alerta")) {
        emit_alert();
    }

    // Atualiza o conteúdo da página com base no estado dos componentes
    create_http_response();

    // Envia a resposta HTTP
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);
    tcp_sent(tpcb, http_sent_callback);

    // Libera o buffer recebido
    pbuf_free(p);

    return ERR_OK;
}

/**
 * @brief Callback de conexão.
 *
 * Esta função é chamada quando uma nova conexão TCP é estabelecida.
 *
 * @param arg Ponteiro para os dados de contexto fornecidos pela aplicação.
 * @param newpcb Ponteiro para a estrutura TCP que representa a nova conexão.
 * @param err Código de erro que pode indicar falhas na criação da conexão.
 *
 * @return Código de erro do tipo err_t indicando o resultado da operação.
 */
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);  // Associa o callback HTTP
    return ERR_OK;
}

/**
 * @brief Inicia o servidor HTTP.
 *
 * Esta função configura e inicia o servidor HTTP responsável por gerenciar as requisições 
 * recebidas, permitindo a interação remota com o sistema de automação residencial. 
 * A implementação abrange os processos de inicialização necessários para estabelecer 
 * a comunicação via protocolo HTTP.
 */
static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }

    // Liga o servidor na porta 80
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }

    pcb = tcp_listen(pcb);  // Coloca o PCB em modo de escuta
    tcp_accept(pcb, connection_callback);  // Associa o callback de conexão

    printf("Servidor HTTP rodando na porta 80...\n");
}


/**
 * Atribui uma cor RGB a um LED.
 */
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
    leds[index].R = r;
    leds[index].G = g;
    leds[index].B = b;
}
 
void npClear() {
    for (uint i = 0; i < LED_COUNT; ++i)
        npSetLED(i, 0, 0, 0);
}

void npWrite() {
    for (uint i = 0; i < LED_COUNT; ++i) {
        pio_sm_put_blocking(np_pio, sm, leds[i].G);
        pio_sm_put_blocking(np_pio, sm, leds[i].R);
        pio_sm_put_blocking(np_pio, sm, leds[i].B);
    }
    sleep_us(100);
}
  
void npInit(uint pin) {
    // Cria programa PIO.
    uint offset = pio_add_program(pio0, &ws2818b_program);
    np_pio = pio0;

    // Toma posse de uma máquina PIO.
    sm = pio_claim_unused_sm(np_pio, false);
    if (sm < 0) {
        np_pio = pio1;
        sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
    }

    // Inicia programa na máquina PIO obtida.
    ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

    npClear();
    npWrite(); // Limpa os LEDs.
}

void turn_on_light(bool *light, uint index) {
    *light = true;
    // npSetLED(index, 255, 255, 255);
    // npWrite();
}

void turn_off_light(bool *light, uint index) {
    *light = false;
    // npSetLED(index, 0, 0, 0);
    // npWrite();
}

void gate(bool state) {
    things.gate = state;

    if (state) {
        printf("Portão aberto\n");
    } else {
        printf("Portão fechado\n");
    }
}

void emit_alert() {
    printf("Alerta acionado\n");
}

// Função para configurar o PWM
void setup_pwm(uint pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM); // Configura o pino como saída PWM
    uint slice_num = pwm_gpio_to_slice_num(pin); // Obtém o número do slice PWM
    pwm_set_wrap(slice_num, PWM_WRAP); // Define o período do PWM para 20ms
    pwm_set_clkdiv(slice_num, 125.0f); // Configuração do divisor de clock para atingir 50Hz
    pwm_set_enabled(slice_num, true); // Habilita o PWM
}

// Função para definir o ciclo ativo (duty cycle) do servo em microssegundos
void set_servo_position(uint pin, uint pulse_width) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_set_gpio_level(pin, pulse_width); // Define o nível PWM
}

// Função para ler os dados do sensor DHT11
int read_dht11(DHT11 *sensor) {
    // Simulação de leitura do DHT11
    sensor->temperature = 25; // Temperatura simulada
    sensor->humidity = 60; // Umidade simulada
    return 0; // Retorna 0 para indicar sucesso
}