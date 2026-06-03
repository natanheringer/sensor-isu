# ISU TRONIK — Lista de Compras e Guia de Montagem
### Versão 3.0 — 29/03/2026

Fontes: BOM original (spec v1.0), pesquisa Thales (encapsulamento), pesquisa Murilo (placa + conectividade), pesquisa complementar (sensores).

---

## A. COMPONENTES ELETRÔNICOS

### 1. Placa principal — ESP32 DOIT DevKit V1
_________________________________________________________________________________________
|         Campo        |                             Detalhe                            |
|----------------------|----------------------------------------------------------------|
|          Modelo      |                ESP DOIT DevKit V1 / ESP32 DevKit V1            |
| Placa no Arduino IDE |                         ESP32 Dev Module                       |
|    Chip principal    |         ESP32-WROOM-32 (dual-core 240MHz, Wi-Fi, Bluetooth)    |
|   Comunicação do MVP |                    Wi-Fi via rede local                        |
|      USB serial      | Micro-USB (chip CP2102 ou CH340, conforme fabricante do clone) |
|        Bateria       | 18650 externa com módulo carregador/regulador adequado         |
|       Tensão         |                    3.3V (regulador onboard)                    |
|   Dimensões da placa |                           ~55×28mm                             |
|   Link Mercado Livre | https://www.mercadolivre.com.br/esp32-doit-devkit-com-esp32-wroom-32-kit-2-unidades-original/p/MLB2077865835 |
|______________________|________________________________________________________________|

**Preço e onde comprar:**
__________________________________________________________________________________________________
|            Loja              |   Preço  |   Prazo    |                   Link                  |
|------------------------------|----------|------------|-----------------------------------------|
| Mercado Livre / lojas locais | R$ 35-70 | 1-7 dias   |      Buscar "ESP32 DOIT DevKit V1"      |
| AliExpress (revendedores)    | US$ 3-7  | 15-30 dias | Buscar "ESP32 DevKit V1 ESP32-WROOM-32" |
|______________________________|__________|____________|_________________________________________|

**Pinagem que vamos usar no MVP:**
___________________________________________________________________________
|    Função   | GPIO |                       Observação                   |
|-------------|------|----------------------------------------------------|
|    I2C SDA  |  21  |      Barramento comum dos 3 VL53L0X e MPU6050      |
|    I2C SCL  |  19  |      Barramento comum dos 3 VL53L0X e MPU6050      |
|  ToF1 XSHUT |  25  |  Controle para inicializar endereço I2C individual |
|  ToF2 XSHUT |  26  |  Controle para inicializar endereço I2C individual |
|  ToF3 XSHUT |  27  |  Controle para inicializar endereço I2C individual |
| MPU6050 INT |  33  |          Interrupção de movimento / wake           |
| Bateria ADC |  34  |  Entrada analógica; usar divisor resistivo externo |
|_________________________________________________________________________|

**Pinos livres alternativos (caso mude o desenho no futuro):**
___________________________________________________________________________
|       GPIO       |     Tipo    |               Observação               |
|------------------|-------------|----------------------------------------|
|        32        | Digital/ADC |                Livre                   |
|        35        | Entrada/ADC |            Livre, somente entrada      |
|       36, 39     | Entrada/ADC |          Livres, somente entrada       |
|     23, 19, 18   |     VSPI    | Livres para expansão SPI se necessário |
|_________________________________________________________________________|

**Notas importantes:**
- No Arduino IDE, selecionar a placa **ESP32 Dev Module**.
- O MVP usa Wi-Fi 2.4GHz e não depende de rede móvel.
- GPIO34 é somente entrada, por isso é adequado para leitura de bateria via ADC e não deve ser usado como saída.

---

### 2. Sensor de distância — VL53L0X (×3 unidades)
_______________________________________________________________________________________
|          Campo          |                           Detalhe                         |
|-------------------------|-----------------------------------------------------------|
|     Modelo do módulo    |                       GY-VL53L0XV2                        |
|           Chip          |          ST VL53L0X (Time-of-Flight laser 940nm)          |
|        Interface        |       I2C (endereço padrão 0x29, alterável via XSHUT)     |
|          Tensão         |              3.3V - 5V (regulador no breakout)            |
|        Pino XSHUT       |             Sim, presente no breakout GY-VL53L0XV2        |
|  Dimensões do módulo    |                         ~13×18mm                          |
|        Quantidade       |                             3                             |
|    Preço por unidade    |                        ~R$ 10-15                          |
|       Custo total       |                        ~R$ 30-45                          |
|      Link mercado livre | https://www.mercadolivre.com.br/sensor-de-distancia-laser-vl53l0x-timeofflight-gyvl53/up/MLBU1981964723            |                                                           |
|_________________________|___________________________________________________________|

**Alcance real por tipo de superfície:**
____________________________________________________________
|              Superfície              | Alcance estimado |
|--------------------------------------|------------------|
|          Alvo branco/claro           |   1800-2000mm    |
|         Metal cinza/alumínio         |   1200-1500mm    |
|       Plástico preto (gabinete)      |    800-1200mm    |
| Superfície muito escura ou angulada  |      ~600mm      |
|______________________________________|__________________|

Precisão: ±3% em condições boas, até ±10% em condições ruins.

---

### 3. Acelerômetro — MPU6050 (×1 unidade)
________________________________________________________________________________
|          Campo          |                         Detalhe                     |
|-------------------------|-----------------------------------------------------|
|     Modelo do módulo    |                         GY-521                      |
|           Chip          | InvenSense MPU6050 (acelerômetro + giroscópio)      |
|        Interface        |       I2C (endereço 0x68, não conflita com VL53L0X) |
|          Tensão         |                       3.3V - 5V                     |
|          Função         |       Wake-on-impact (detectar descarte no coletor) |
| Threshold recomendado   |             40-80mg (calibrar em teste real)        |
|         Pino INT        |              Sim, disponível no breakout GY-521     |
|      Conexão wake       |                       INT → GPIO33                  |
|        Dimensões        |                         ~20×16mm                    |
|          Preço          |                        ~R$ 10-15                    |
|     Link mercado livre  | https://www.mercadolivre.com.br/mpu6050-gy521-modulo-sensor-acelerometro-e-giroscopio-gy521/up/MLBU1963901751?pdp_filters=item_id:MLB1922679958                                |
|_________________________|_____________________________________________________|

---

### 4. Bateria 18650 Li-Ion (×1 unidade)
_________________________________________________________________________________
|        Campo        |                         Detalhe                         |
|---------------------|---------------------------------------------------------|
|         Tipo        |                18650 Li-Ion recarregável                |
|     Capacidade      |            Mínimo 2600mAh, recomendado 3000mAh          |
|        Tensão       |            3.7V nominal (4.2V cheio, 3.0V corte)        |
| Modelos confiáveis  |            Samsung 30Q, LG HG2, Panasonic NCR18650B     |
|        Preço        |                         R$ 25-40                        |
|         Onde        | Mercado Livre — buscar "bateria 18650 3000mAh original" |
|_____________________|_________________________________________________________|

Usar holder 18650 externo e módulo de carga/regulação compatível com ESP32. Autonomia estimada será validada no MVP com ciclo de 30 min.

---

## B. CONECTIVIDADE

### 5. Wi-Fi do MVP
_________________________________________________________________________________
|        Campo        |                              Detalhe                    |
|---------------------|---------------------------------------------------------|
|      Tecnologia     |                       Wi-Fi 2.4GHz do ESP32             |
|     Bibliotecas     |                  WiFi, HTTPClient, ArduinoJson          |
|    Configuração     |      SSID e senha no firmware ou configuração local     |
| Consumo estimado    | Reduzir com ciclo curto de conexão, POST e deep sleep   |
|_____________________|_________________________________________________________|

**Recomendação para MVP:**
- Usar uma rede Wi-Fi conhecida no local de teste ou hotspot durante a validação em campo.
- Validar RSSI no ponto real do coletor antes de fechar a caixa.
- O firmware deve conectar ao Wi-Fi, fazer o POST HTTP, encerrar a conexão e voltar ao deep sleep.

---

## C. ENCAPSULAMENTO E MECÂNICA (pesquisa Thales)

### 6. Caixa IP65 policarbonato transparente
__________________________________________________________________
|        Campo        |                  Detalhe                 |
|---------------------|------------------------------------------|
|       Material      |          Policarbonato transparente      |
|    Classificação    |                   IP65                   |
| Dimensões sugeridas |               ~130×80×70mm               |
|         Link        | https://share.google/GBH64IcwtCDfZq9R1   |
|        Preço        |                 R$ 25-35                 |
|_____________________|__________________________________________|

### 7. Prensa-cabos PG7 IP68 (×2)
_______________________________________________________________
|     Campo    |                    Detalhe                   |
|--------------|----------------------------------------------|
|    Função    |    Passagem vedada para cabo de energia      |
|  Quantidade  |                       2                      |
|     Link     | https://share.google/oo6GZiZgthNs3Uo2F       |
|    Preço     |                 R$ 8-12 (par)                |
|______________|______________________________________________|

### 8. Silicone RTV neutro
________________________________________________________________________
|      Campo      |                         Detalhe                    |
|-----------------|----------------------------------------------------|
|       Tipo      |          Neutro (incolor ou branco, nunca ácido)   |
|        Uso      |          Vedação das janelas de acrílico dos ToF   |
|       Link      | https://produto.mercadolivre.com.br/MLB-3790058230 |
|   Alternativa   |          Qualquer silicone RTV neutro de ferragem  |
|      Preço      |                         R$ 15-20                   |
|_________________|____________________________________________________|

### 9. Acrílico cristal PMMA 2mm
___________________________________________________________________________________
|          Campo          |                         Detalhe                       |
|-------------------------|-------------------------------------------------------|
|         Material        |                 PMMA cristal transparente             |
|        Espessura        |                           2mm                         |
|  Transmitância em 940nm |                    ~92% (confirmado)                  |
|    Corte necessário     |                    3 discos de ~10mm                  |
|           Link          | Mercado Livre — "chapa acrílico cristal 20x30cm 2mm"  |
|          Preço          |                  R$ 10-15 (chapa 20×30cm)             |
|_________________________|_______________________________________________________|

**Cross-talk (alerta Thales):** encostar o acrílico no sensor e/ou usar o-ring ou espuma preta isolando emissor e receptor.

### 10. Fixação
__________________________________________________________________________________________
|                  Item                  |              Detalhe              |   Preço   |
|----------------------------------------|-----------------------------------|-----------|
| Parafusos inox M3×10mm + porca (20un)  |      Link Thales: Mercado Livre   |  ~R$ 10   |
|          Espaçadores nylon M3          |    Ferragem ou loja de eletrônica |  ~R$ 5    |
|      Zip ties industriais pretos       |       https://a.co/d/0dP30Ao9     | ~R$ 10-15 |
|________________________________________|___________________________________|___________|

Fixação por material do coletor: metal → inox M3 ou Araldite; plástico/PVC → zip ties UV; madeira → soberbos.

### 11. Fios, conectores e acessórios
___________________________________________________________________________________
|                Item                |              Detalhe             |  Preço  |
|------------------------------------|----------------------------------|---------|
|     Fios jumper Dupont F-F 20cm    |             Pacote 20+           |  ~R$ 8  |
|          Fios silicone AWG26       |             1m por cor           | ~R$ 10  |
| Conectores JST-PH 2.0mm 4 pinos    |        Conexões removíveis       |  ~R$ 8  |
|      Cabo micro-USB curto 30cm     |           Carga/serviço          | ~R$ 10  |
|       Cap borracha micro-USB       | "micro USB dust plug silicone"   |  ~R$ 5  |
|____________________________________|__________________________________|_________|

---

## D. RESUMO FINANCEIRO (atualizado)

| Categoria | Custo estimado |
|-----------|---------------|
| Eletrônicos (ESP32 DevKit V1 + 3×VL53L0X + MPU6050 + bateria + módulo de carga/regulação) | R$ 125-210 |
| Conectividade Wi-Fi | R$ 0 |
| Encapsulamento (caixa + prensa-cabos + silicone + acrílico + fixação) | R$ 70-95 |
| Acessórios (fios + conectores + USB + zip ties) | R$ 40-55 |
| **TOTAL estimado** | **R$ 235-360** |

---

## E. PRIORIDADE DE COMPRA

**Pedido 1 — AliExpress ou loja oficial (fazer agora):**
- 1× ESP32 DOIT DevKit V1 / ESP32 DevKit V1
- 3× VL53L0X GY-VL53L0XV2
- 1× MPU6050 GY-521
- 1× Kit fios jumper Dupont

**Pedido 2 — Mercado Livre / lojas locais (pode esperar 1 semana):**
- 1× Bateria 18650 3000mAh
- 1× Holder 18650
- 1× Módulo de carga/regulação adequado para alimentar ESP32
- 1× Caixa IP65 policarbonato
- 2× Prensa-cabo PG7
- 1× Silicone RTV neutro
- 1× Chapa acrílico 2mm
- Parafusos M3 inox + espaçadores nylon
- Zip ties industriais

**Pedido 3 — Rede de teste:**
- Separar SSID e senha de uma rede Wi-Fi 2.4GHz para bancada.
- Para campo, confirmar se haverá Wi-Fi no local ou hotspot dedicado durante o teste.

---

## F. INSTRUÇÕES DE MONTAGEM

### Etapa 1 — Teste individual em bancada

Conectar o ESP32 DOIT DevKit V1 no computador via micro-USB. Instalar driver CP2102 ou CH340 se necessário. Verificar que a placa aparece como porta COM.

No Arduino IDE, selecionar **ESP32 Dev Module**.

Instalar bibliotecas no Arduino IDE: WiFi, HTTPClient, ArduinoJson, Adafruit VL53L0X, Adafruit MPU6050, Adafruit Unified Sensor. WiFi e HTTPClient vêm do core ESP32.

Conectar 1 VL53L0X no I2C (SDA=21, SCL=19, VIN=3.3V, GND=GND). Rodar exemplo da biblioteca. Apontar para a mesa e verificar distância.

Conectar o MPU6050 no I2C. Rodar exemplo. Sacudir e ver os valores mudarem.

Inserir bateria 18650. Verificar LED de carga (vermelho = carregando, verde = cheio). Desconectar USB e confirmar que funciona só com bateria.

### Etapa 2 — Multi-ToF com XSHUT

Conectar os 3 VL53L0X em paralelo no I2C (compartilham SDA e SCL). Conectar XSHUT: ToF1→GPIO25, ToF2→GPIO26, ToF3→GPIO27.

Inicialização sequencial no firmware:
1. Puxar todos XSHUT para LOW (desliga todos)
2. XSHUT1 HIGH, esperar 50ms, tof1.begin(0x30)
3. XSHUT2 HIGH, esperar 50ms, tof2.begin(0x31)
4. XSHUT3 HIGH, esperar 50ms, tof3.begin(0x32)

Ler os 3 sensores. Colocar objetos a distâncias conhecidas e validar.

### Etapa 3 — Deep sleep e wake

Conectar INT do MPU6050 ao GPIO33 do ESP32.

Configurar Motion Detection Interrupt com threshold ~50mg.

No firmware:
- esp_sleep_enable_timer_wakeup(30 min em µs)
- esp_sleep_enable_ext0_wakeup(GPIO33, HIGH)
- esp_deep_sleep_start()

Verificar no Serial Monitor: acorda por timer e por impacto (bater na mesa).

### Etapa 4 — Conexão Wi-Fi

Configurar SSID e senha da rede Wi-Fi 2.4GHz no firmware.

O teste de bancada mais importante do projeto: conectar ao Wi-Fi, fazer POST HTTP para o backend e voltar ao deep sleep. Se isso funcionar, a integração do dispositivo com a dashboard está validada para o MVP.

### Etapa 5 — Montagem na caixa

Marcar 3 furos na face inferior (triângulo, ~120° entre eles, ~35mm entre centros). Furar com broca 10mm. Lixar bordas.

Colar discos de acrílico 2mm por fora com silicone RTV. Esperar 24h de cura.

Montar VL53L0X por dentro, encostados no acrílico. Se houver cross-talk, adicionar espuma preta.

Fixar ESP32 DevKit V1 com espaçadores nylon M3. Fixar MPU6050 em contato rígido com a parede da caixa.

Passar cabo USB ou extensão de carga/serviço pelos prensa-cabos PG7 se necessário. Apertar.

Fechar caixa. Testar vedação com borrifo de água.

### Etapa 6 — Instalação no coletor

Fixar caixa na parte superior interna do coletor, sensores ToF apontando para baixo. Método conforme material (seção 10).

Antes de instalar: verificar sinal Wi-Fi no local exato do coletor.
