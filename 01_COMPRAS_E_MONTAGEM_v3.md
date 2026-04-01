# ISU TRONIK — Lista de Compras e Guia de Montagem
### Versão 3.0 — 29/03/2026

Fontes: BOM original (spec v1.0), pesquisa Thales (encapsulamento), pesquisa Murilo (placa + conectividade), pesquisa complementar (sensores).

---

## A. COMPONENTES ELETRÔNICOS

### 1. Placa principal — LilyGO TTGO T-SIM7000G

| Campo | Detalhe |
|-------|---------|
| Modelo | LILYGO® TTGO T-SIM7000G ESP32-WROVER-B |
| Revisão confirmada | V1.1 (verificar fotos do anúncio contra diagrama V1.1 antes de comprar) |
| Chip principal | ESP32-WROVER-B (dual-core 240MHz, WiFi, Bluetooth, 8MB PSRAM) |
| Modem | SIM7000G (2G GSM + NB-IoT + LTE Cat-M1 + GPS/GNSS) |
| Chip serial USB | CH9102 |
| Bateria | Holder 18650 integrado no PCB + circuito de carga USB-C + conector JST 2 pinos para painel solar |
| SIM | Nano SIM exclusivamente (diagrama V1.1 explicita "Only Supports Nano SIM Card") |
| Slot microSD | Sim (TF card via SPI) — não usaremos no MVP |
| Tensão | 3.3V (regulador onboard) |
| Dimensões da placa | ~100×32mm (com bateria: ~100×32×25mm) |

**Preço e onde comprar:**

| Loja | Preço | Prazo | Link |
|------|-------|-------|------|
| LILYGO oficial | US$ 35,14 | DHL/FedEx 7-10 dias, Standard 10-25 dias | https://lilygo.cc/products/t-sim7000g |
| LILYGO no Tindie | US$ 34,62 | Variável | https://www.tindie.com/products/lilygo/lilygo-ttgo-t-sim7000g-sim-development-board/ |
| AliExpress (revendedores) | US$ 18-25 | 15-30 dias | https://www.aliexpress.com/item/1005003223833108.html |

Repositório oficial: https://github.com/Xinyuan-LilyGO/LilyGO-T-SIM7000G

**Diagrama de pinagem V1.1:**
https://github.com/Xinyuan-LilyGO/LilyGO-T-SIM7000G/raw/master/Historical/SIM7000G_20200415/pins.jpg

**Pinagem que vamos usar no MVP:**

| Função | GPIO | Observação |
|--------|------|------------|
| I2C SDA | 21 | Wire_SDA (rotulado no diagrama V1.1) |
| I2C SCL | 22 | Wire_SCL (rotulado no diagrama V1.1) |
| ToF1 XSHUT | 13 | Conflita com microSD CS — não usar microSD no MVP |
| ToF2 XSHUT | 14 | Conflita com microSD SCLK — não usar microSD no MVP |
| ToF3 XSHUT | 15 | Conflita com microSD MOSI — não usar microSD no MVP |
| MPU6050 INT | 2 | Conflita com microSD MISO — não usar microSD no MVP |
| Bateria ADC | 35 | Divisor resistivo 100k/100k (já na placa) |

**Pinos ocupados pelo modem (não compartilhar):**

| Função | GPIO |
|--------|------|
| Modem TX | 26 |
| Modem RX | 27 |
| Modem POWER | 4 |
| Serial debug TX | 1 |
| Serial debug RX | 3 |

**Pinos livres alternativos (caso mude o desenho no futuro):**

| GPIO | Tipo | Observação (diagrama V1.1) |
|------|------|---------------------------|
| 32 | Digital | Livre |
| 33 | Digital | Livre |
| 34 | Digital (entrada) | Livre |
| 39 | ADC (entrada) | Livre, útil para leitura analógica |
| 23, 19, 18 | VSPI | Livre para SPI dedicado se necessário |

**Decisão documentada:** no MVP, os GPIOs 2, 13, 14, 15 são usados para sensores (XSHUT e INT). Isso impede o uso simultâneo do slot microSD. Se no futuro precisarmos de datalog local (buffer em SD quando sem rede), será necessário remapear os pinos dos sensores para 32, 33, 34 ou usar multiplexador I2C.

**Notas importantes:**
- Conferir que o anúncio é revisão V1.1 comparando fotos com o diagrama oficial antes de finalizar a compra.
- O SIM7000G não suporta 4G convencional. Opera em 2G (GSM/GPRS) e NB-IoT/LTE Cat-M1.
- Antena IPEX LTE/GPS normalmente inclusa na compra.

---

### 2. Sensor de distância — VL53L0X (×3 unidades)

| Campo | Detalhe |
|-------|---------|
| Modelo do módulo | GY-VL53L0XV2 |
| Chip | ST VL53L0X (Time-of-Flight laser 940nm) |
| Interface | I2C (endereço padrão 0x29, alterável via XSHUT) |
| Tensão | 3.3V - 5V (regulador onboard no breakout) |
| Pino XSHUT | Sim, presente no breakout GY-VL53L0XV2 |
| Dimensões do módulo | ~13×18mm |
| Quantidade | 3 |
| Preço por unidade | ~R$ 10-15 |
| Custo total | ~R$ 30-45 |
| Link AliExpress | https://pt.aliexpress.com/item/1005003354879961.html |

**Alcance real por tipo de superfície:**

| Superfície | Alcance estimado |
|------------|-----------------|
| Alvo branco/claro | 1800-2000mm |
| Metal cinza/alumínio | 1200-1500mm |
| Plástico preto (gabinete) | 800-1200mm |
| Superfície muito escura ou angulada | ~600mm |

Precisão: ±3% em condições boas, até ±10% em condições ruins.

---

### 3. Acelerômetro — MPU6050 (×1 unidade)

| Campo | Detalhe |
|-------|---------|
| Modelo do módulo | GY-521 |
| Chip | InvenSense MPU6050 (acelerômetro + giroscópio 6 eixos) |
| Interface | I2C (endereço 0x68 — não conflita com VL53L0X) |
| Tensão | 3.3V - 5V |
| Função | Wake-on-impact (detectar descarte no coletor) |
| Threshold recomendado | 40-80mg (calibrar em teste real) |
| Pino INT | Sim, disponível no breakout GY-521 |
| Conexão wake | INT → GPIO2 (suporta ext0 wakeup deep sleep) |
| Dimensões | ~20×16mm |
| Preço | ~R$ 10-15 |
| Link AliExpress | https://pt.aliexpress.com/item/1005003340292792.html |

---

### 4. Bateria 18650 Li-Ion (×1 unidade)

| Campo | Detalhe |
|-------|---------|
| Tipo | 18650 Li-Ion recarregável |
| Capacidade | Mínimo 2600mAh, recomendado 3000mAh |
| Tensão | 3.7V nominal (4.2V cheio, 3.0V corte) |
| Modelos confiáveis | Samsung 30Q, LG HG2, Panasonic NCR18650B |
| Preço | R$ 25-40 |
| Onde | Mercado Livre — buscar "bateria 18650 3000mAh original" |

Holder não precisa (já integrado na placa). Autonomia estimada: 6-8 semanas com ciclo de 30 min.

---

## B. CONECTIVIDADE

### 5. Chip de celular M2M

| Campo | Detalhe |
|-------|---------|
| Tipo de SIM | Nano SIM (4FF) — único formato aceito pela placa |
| Tecnologia | 2G (GPRS) ou NB-IoT/LTE Cat-M1 |
| Consumo estimado | ~1.5KB/transmissão × 48/dia = ~72KB/dia = ~2.2MB/mês |

**Operadoras pesquisadas (Murilo + complementar):**

| Operadora | Franquia | Preço mensal | APN | Tecnologias | Contratação | Fonte |
|-----------|----------|-------------|-----|-------------|-------------|-------|
| Vivo M2M | 20MB | R$ 6,90 | APN privado (definido no contrato) ou zap.vivo.com.br (chip comum) | 2G, LTE-M, NB-IoT | Portal empresas (pode exigir CNPJ) | Pesquisa Murilo |
| Vivo M2M | 50MB | R$ 9,90 | Idem | Idem | Idem | Pesquisa Murilo |
| Vivo M2M | 100MB | R$ 11,90 | Idem | Idem | Idem | Pesquisa Murilo |
| Claro M2M (com gestão) | 2MB | R$ 20,50 | APN pública específica Cat-M (pedir à operadora) | 2G, LTE-M | Portal empresas, permanência 24 meses | Pesquisa Murilo |
| Claro M2M (com gestão) | 20MB | R$ 22,50 | Idem | Idem | Idem | Pesquisa Murilo |
| Datatem (MVNO) | 20MB | ~R$ 7-12 | APN privada (fornecida na ativação) | Multioperadora (Vivo/Claro/TIM) | Sem CNPJ para teste, entrega 72h | Pesquisa complementar |
| emnify (MVNO) | Pay-per-use | ~US$ 0.50/mês | Fornecida no painel | NB-IoT, LTE-M, 2G | Chip teste grátis 60 dias, ativa online | Pesquisa complementar |
| 1NCE | 500MB total | US$ 14 único | iot.1nce.net | NB-IoT, LTE-M, 2G | Pagamento único, sem mensalidade, 10 anos | Pesquisa complementar |

**Pontos levantados pelo Murilo que afetam a decisão:**

1. O APN de chip M2M corporativo não é o mesmo do celular comum. A Vivo tem APN privado definido no contrato, a Claro tem APNs específicas para Cat-M. A pergunta certa para o comercial da operadora é: "meu chip é APN pública padrão ou APN dedicada? qual o APN exato + usuário/senha + tipo de autenticação?"

2. Ter 4G na cidade não garante NB-IoT ou Cat-M no ponto do coletor. Antes de escolher operadora, verificar cobertura IoT no endereço exato usando os mapas:
   - Vivo: https://mapadecobertura.vivo.com.br/ (alternar camada IOT/NBIOT/CATM)
   - Claro: https://www.claro.com.br/faq/internet-residencial/mapa-de-cobertura-lot

3. A validação mais rápida é um teste de bancada de 30 minutos com chip ativo e APN confirmado. O que esse teste valida não é o ESP32, é rede + APN + modem funcionando juntos.

**Recomendação para MVP:**
- Mais rápido para protótipo: emnify (grátis 60 dias, ativa online, sem burocracia)
- Mais barato longo prazo: Vivo 20MB a R$ 6,90/mês (se tiver CNPJ) ou 1NCE US$14 único
- A Claro é significativamente mais cara e com permanência de 24 meses — não recomendada para MVP acadêmico

---

## C. ENCAPSULAMENTO E MECÂNICA (pesquisa Thales)

### 6. Caixa IP65 policarbonato transparente

| Campo | Detalhe |
|-------|---------|
| Material | Policarbonato transparente |
| Classificação | IP65 |
| Dimensões sugeridas | ~130×80×70mm |
| Link | https://share.google/GBH64IcwtCDfZq9R1 |
| Preço | R$ 25-35 |

### 7. Prensa-cabos PG7 IP68 (×2)

| Campo | Detalhe |
|-------|---------|
| Função | Passagem vedada para antena e USB-C |
| Quantidade | 2 |
| Link | https://share.google/oo6GZiZgthNs3Uo2F |
| Preço | R$ 8-12 (par) |

### 8. Silicone RTV neutro

| Campo | Detalhe |
|-------|---------|
| Tipo | Neutro (incolor ou branco — nunca ácido) |
| Uso | Vedação das janelas de acrílico dos ToF |
| Link | https://produto.mercadolivre.com.br/MLB-3790058230 |
| Alternativa | Qualquer silicone RTV neutro de ferragem |
| Preço | R$ 15-20 |

### 9. Acrílico cristal PMMA 2mm

| Campo | Detalhe |
|-------|---------|
| Material | PMMA cristal transparente |
| Espessura | 2mm |
| Transmitância em 940nm | ~92% (confirmado) |
| Corte necessário | 3 discos de ~10mm |
| Link | Mercado Livre — "chapa acrílico transparente cristal 20x30cm 2mm" |
| Preço | R$ 10-15 (chapa 20×30cm) |

**Cross-talk (alerta Thales):** encostar o acrílico no sensor e/ou usar o-ring ou espuma preta isolando emissor e receptor.

### 10. Fixação

| Item | Detalhe | Preço |
|------|---------|-------|
| Parafusos inox M3×10mm + porca (20un) | Link Thales: Mercado Livre | ~R$ 10 |
| Espaçadores nylon M3 | Ferragem ou loja de eletrônica | ~R$ 5 |
| Zip ties industriais pretos (UV) | https://a.co/d/0dP30Ao9 | ~R$ 10-15 |

Fixação por material do coletor: metal → inox M3 ou Araldite; plástico/PVC → zip ties UV; madeira → soberbos.

### 11. Fios, conectores e acessórios

| Item | Detalhe | Preço |
|------|---------|-------|
| Fios jumper Dupont F-F 20cm | Pacote 20+ | ~R$ 8 |
| Fios silicone AWG26 | 1m por cor | ~R$ 10 |
| Conectores JST-PH 2.0mm 4 pinos | Conexões removíveis | ~R$ 8 |
| Cabo USB-C curto 30cm | Carga/serviço | ~R$ 10 |
| Cap borracha USB-C | "USB-C dust plug silicone" | ~R$ 5 |

---

## D. RESUMO FINANCEIRO (atualizado)

| Categoria | Custo estimado |
|-----------|---------------|
| Eletrônicos (LilyGO loja oficial + 3×VL53L0X + MPU6050 + bateria) | R$ 260-310 |
| Eletrônicos (LilyGO AliExpress + 3×VL53L0X + MPU6050 + bateria) | R$ 185-225 |
| Conectividade (chip M2M primeiro período) | R$ 0-80 |
| Encapsulamento (caixa + prensa-cabos + silicone + acrílico + fixação) | R$ 70-95 |
| Acessórios (fios + conectores + USB + zip ties) | R$ 40-55 |
| **TOTAL (loja oficial)** | **R$ 370-540** |
| **TOTAL (AliExpress)** | **R$ 295-455** |

---

## E. PRIORIDADE DE COMPRA

**Pedido 1 — AliExpress ou loja oficial (fazer agora):**
- 1× LilyGO T-SIM7000G (conferir revisão V1.1 nas fotos antes de finalizar)
- 3× VL53L0X GY-VL53L0XV2
- 1× MPU6050 GY-521
- 1× Kit fios jumper Dupont

**Pedido 2 — Mercado Livre / lojas locais (pode esperar 1 semana):**
- 1× Bateria 18650 3000mAh
- 1× Caixa IP65 policarbonato
- 2× Prensa-cabo PG7
- 1× Silicone RTV neutro
- 1× Chapa acrílico 2mm
- Parafusos M3 inox + espaçadores nylon
- Zip ties industriais

**Pedido 3 — Chip celular (quando a placa chegar):**
- Solicitar chip emnify (grátis 60 dias) para protótipo
- Antes de ativar: verificar cobertura IoT no endereço do coletor via mapas das operadoras
- Ao receber o chip: confirmar APN exato com a operadora antes de configurar no firmware

---

## F. INSTRUÇÕES DE MONTAGEM

### Etapa 1 — Teste individual em bancada

Conectar a LilyGO no computador via USB-C. Instalar driver CH9102 se necessário. Verificar que a placa aparece como porta COM.

Instalar bibliotecas no Arduino IDE: TinyGSM, ArduinoJson, Adafruit VL53L0X, Adafruit MPU6050, Adafruit Unified Sensor.

Conectar 1 VL53L0X no I2C (SDA=21, SCL=22, VIN=3.3V, GND=GND). Rodar exemplo da biblioteca. Apontar para a mesa e verificar distância.

Conectar o MPU6050 no I2C. Rodar exemplo. Sacudir e ver os valores mudarem.

Inserir bateria 18650. Verificar LED de carga (vermelho = carregando, verde = cheio). Desconectar USB e confirmar que funciona só com bateria.

### Etapa 2 — Multi-ToF com XSHUT

Conectar os 3 VL53L0X em paralelo no I2C (compartilham SDA e SCL). Conectar XSHUT: ToF1→GPIO13, ToF2→GPIO14, ToF3→GPIO15.

Nota: esses GPIOs são compartilhados com o slot microSD. Não inserir cartão microSD enquanto os sensores estiverem conectados.

Inicialização sequencial no firmware:
1. Puxar todos XSHUT para LOW (desliga todos)
2. XSHUT1 HIGH, esperar 50ms, tof1.begin(0x30)
3. XSHUT2 HIGH, esperar 50ms, tof2.begin(0x31)
4. XSHUT3 HIGH, esperar 50ms, tof3.begin(0x32)

Ler os 3 sensores. Colocar objetos a distâncias conhecidas e validar.

### Etapa 3 — Deep sleep e wake

Conectar INT do MPU6050 ao GPIO2 do ESP32. (Também compartilhado com microSD MISO — não usar microSD.)

Configurar Motion Detection Interrupt com threshold ~50mg.

No firmware:
- esp_sleep_enable_timer_wakeup(30 min em µs)
- esp_sleep_enable_ext0_wakeup(GPIO2, HIGH)
- esp_deep_sleep_start()

Verificar no Serial Monitor: acorda por timer e por impacto (bater na mesa).

### Etapa 4 — Conexão celular

Inserir nano SIM no slot (placa desligada). Configurar APN no firmware conforme operadora (APN confirmado com a operadora, não APN genérico).

O teste de bancada mais importante do projeto: ligar modem, conectar GPRS/NB-IoT, fazer POST para o backend. Se isso funcionar, o gargalo real está resolvido.

### Etapa 5 — Montagem na caixa

Marcar 3 furos na face inferior (triângulo, ~120° entre eles, ~35mm entre centros). Furar com broca 10mm. Lixar bordas.

Colar discos de acrílico 2mm por fora com silicone RTV. Esperar 24h de cura.

Montar VL53L0X por dentro, encostados no acrílico. Se houver cross-talk, adicionar espuma preta.

Fixar LilyGO com espaçadores nylon M3. Fixar MPU6050 em contato rígido com a parede da caixa.

Passar antena e cabo USB pelos prensa-cabos PG7. Apertar.

Fechar caixa. Testar vedação com borrifo de água.

### Etapa 6 — Instalação no coletor

Fixar caixa na parte superior interna do coletor, sensores ToF apontando para baixo. Método conforme material (seção 10).

Antes de instalar: verificar cobertura celular no local com celular pessoal (mesma operadora do chip M2M).
