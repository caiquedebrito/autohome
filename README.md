# AutoHome

## Descrição

AutoHome é uma plataforma de automação residencial baseada no microcontrolador **Raspberry Pi Pico W**. O sistema fornece:

- Monitoramento em tempo real de **temperatura** e **umidade** (sensor DHT11)  
- Controle remoto de **iluminação** (sala, cozinha, quarto)  
- Abertura/fechamento de **portão** via servo motor  
- Emissão de **alertas sonoros** via buzzer  
- Feedback local em **display OLED** (SSD1306) e **matriz de LEDs** (WS2812)  
- Interface web responsiva servida diretamente pelo Pico W  

## Funcionalidade Geral

1. Na inicialização, o Pico W conecta-se à rede Wi-Fi definida e obtém um endereço IP.  
2. Lê temperatura e umidade do DHT11 e exibe no OLED.  
3. Inicia servidor HTTP na porta 80.  
4. Interface web exibe clima e controles de portão, luzes e alertas.  
5. Cliques na página acionam endpoints (`/s_on`, `/p_o` etc.), atualizam estado interno e redesenham a página.  
6. LEDs WS2812 e servo motor podem refletir fisicamente o estado dos dispositivos.  
7. Botão físico com interrupção deposita o Pico em modo bootloader USB para atualização.

---

## Materiais Necessários

- **Raspberry Pi Pico W**  
- Sensor **DHT11** (temperatura + umidade)  
- Display **OLED SSD1306** (I²C, 128×64)  
- Matriz ou tira de **LEDs WS2812**  
- **Servo motor** (p. ex. SG90)  
- **Buzzer** (ativo ou passivo)  
- Botão momentâneo (GPIO IRQ)  
- Resistores de pull-up (separados) ou usar pull-ups internos  
- Cabos jumper e protoboard  
- Fonte 5 V (via USB ou alimentação externa, conforme servo e LEDs)

---

## Pré-requisitos de Software

- [Pico SDK](https://github.com/raspberrypi/pico-sdk) configurado  
- Toolchain C/CMake (GCC for ARM, CMake ≥ 3.13)  
- [pico-extras](https://github.com/raspberrypi/pico-extras) (para drivers SSD1306)  
- Biblioteca **lwIP** e **pico_cyw43_arch** (inclusas no SDK)  
- `picotool` ou método de UF2 drag-and-drop  

---

## Conexões de Hardware

| Componente       | Pico W GPIO            | Notas                                     |
|------------------|------------------------|-------------------------------------------|
| SSD1306 OLED     | SDA → GP14, SCL → GP15 | I²C1, 400 kHz, endereço 0x3C              |
| DHT11            | GP?                    | Data line → o pino configurado            |
| WS2812           | GP7                    | PIO0, programa WS2812@800 kHz             |
| Servo motor      | GP8                    | PWM 50 Hz (wrap 20000, clkdiv 125)        |
| Buzzer           | GP?                    | PWM ou saída digital                      |
| Botão de bootloader | GP6                 | Pull-up interno, IRQ falling edge (debounce) |

> Ajuste o pino do DHT11 e do buzzer conforme necessidade no código.

---

## Compilação e Deploy

```bash
# Clone este repositório
git clone https://github.com/seu-usuario/AutoHome.git
cd AutoHome

# Configure o Pico SDK (exemplo)
export PICO_SDK_PATH=/caminho/para/pico-sdk

# Crie pasta de build
mkdir build && cd build

# Gere arquivos de build
cmake ..

# Compile
make -j4

# Grave no Pico W
# 1) Entre em modo bootloader segurando o botão físico
# 2) Conecte via USB; copie 'firmware.uf2' para o drive 'RPI-RP2'
cp firmware.uf2 /Volumes/RPI-RP2/
```

## Configuração Wi-Fi
Edite os defines no topo de main.c:

```c
#define WIFI_SSID "SEU_SSID"
#define WIFI_PASS "SUA_SENHA"
```

Recompile e refaça o deploy para conectar à sua rede.

## Uso
1. Ligue o Pico W; aguarde conexão Wi-Fi.

2. Observe IP no terminal serial e no OLED.

3. Abra navegador em http://<IP_DO_PICO>:
  - Veja temperatura e umidade
  - Abra/feche o portão
  - Ligue/desligue luzes da sala, cozinha e quarto
  - Acione alerta sonoro

4. LEDs WS2812 e servo motor refletem mudanças (se habilitados no código).

5. Para atualizar firmware, pressione o botão físico para entrar em bootloader USB.