# ISU TRONIK — MVP (Minimum Viable Product)
### Versão 2.0 — 28/03/2026

---

## Objetivo

Provar que o dispositivo ISU, operando dentro de um coletor real da TRONIK, consegue detectar evento de descarte, medir nível de enchimento, e enviar esses dados via Wi-Fi para a dashboard com autonomia aceitável em bateria.

Se o MVP funcionar, a Maiara passa a saber remotamente quais coletores precisam de coleta.

---

## O que precisa funcionar

- Rodar em ESP32 DOIT DevKit V1, configurado no Arduino IDE como ESP32 Dev Module.
- Ler 3 sensores VL53L0X e converter as distâncias em porcentagem de enchimento.
- Detectar impacto com MPU6050 e acordar do deep sleep.
- Acordar também por timer a cada 30 minutos como backup.
- Enviar pacote HTTP via Wi-Fi com nível, bateria, e distâncias brutas.
- O backend receber o pacote, atualizar o coletor na dashboard, e emitir via WebSocket.
- Criar notificação automática se nível passar de 80%.
- Rodar em bateria 18650 por pelo menos alguns dias de teste contínuo.
- Ficar dentro da caixa IP65 com vedação funcional.

## Fora do MVP

- Classificação de tipo de material (plástico vs metal vs magnético).
- OTA (atualização de firmware remota).
- Criptografia TLS no POST.
- Multi-tenant (múltiplos sensores com autenticação por dispositivo).
- Painel solar.
- Interface administrativa para configurar threshold/intervalo remotamente.

---

## Estados do firmware

O firmware opera como máquina de estados simples. Não existe loop contínuo; cada wake reinicia pelo setup().

```
SLEEP (default)
  │
  ├─ timer (30 min) ──┐
  │                    ▼
  └─ impacto (MPU) ──→ WAKE
                        │
                        ▼
                     MEASURE
                     (ler 3 ToF em burst de 5, mediana por sensor,
                      mediana dos 3, converter para %, ler bateria)
                        │
                        ▼
                      SEND
                      (conectar Wi-Fi, POST HTTP,
                       encerrar conexão)
                        │
                        ▼
                     SLEEP
```

Se a bateria estiver abaixo de 5%, pula direto para SLEEP sem transmitir.

Se o POST falhar, volta para SLEEP e tenta no próximo ciclo. Sem buffer local no MVP.

---

## Payload

```json
{
  "api_key": "tronik-isu-mvp-2026",
  "sensor_id": 1,
  "coletor_id": 3,
  "nivel_preenchimento": 63.2,
  "bateria": 87.3,
  "leituras_tof": [420.0, 390.0, 405.0],
  "wake_reason": "impact"
}
```

---

## Plano de montagem e teste (7 dias, quando os componentes chegarem)

Dia 1 — Bancada elétrica básica.
Conectar ESP32 DOIT DevKit V1 no computador. No Arduino IDE, selecionar ESP32 Dev Module. Confirmar boot. 1 VL53L0X no I2C (SDA=21, SCL=22), validar leitura. MPU6050 no I2C, validar leitura. Bateria, confirmar funcionamento sem USB.

Dia 2 — Multi-ToF.
3 sensores VL53L0X com XSHUT nos GPIOs 25/26/27, endereços 0x30/0x31/0x32. Coletar amostras apontando para superfícies variadas. Validar estabilidade.

Dia 3 — Deep sleep e wake.
Ciclo completo: timer wakeup + ext0 wakeup pelo MPU6050 INT no GPIO33. Medir consumo aproximado. Confirmar que impacto na mesa acorda, vibração leve não.

Dia 4 — Wi-Fi.
Configurar SSID e senha da rede 2.4GHz. Fazer primeiro POST para o backend usando WiFi, HTTPClient e ArduinoJson. Tratar falha: 1 retry, se falhar voltar para sleep.

Dia 5 — Backend.
Testar end-to-end: firmware envia → backend recebe → coletor atualiza → WebSocket emite → frontend reflete.

Dia 6 — Caixa e ótica.
Montar na IP65. Furar, colar acrílico, vedar com silicone. Testar cross-talk. Testar vedação.

Dia 7 — Teste em campo.
Instalar no coletor real. Rodar algumas horas. Monitorar pela dashboard. Jogar objetos e verificar que o nível muda.

---

## Critérios de aceite

O MVP será considerado aprovado se:

- A maioria dos ciclos planejados resultar em envio bem-sucedido para o backend. Perdas ocasionais por instabilidade de rede são aceitáveis desde que o dado chegue na tentativa seguinte.
- O wake por impacto funcionar na maioria dos eventos de descarte real. Alguns falsos positivos ou negativos são toleráveis e serão ajustados na calibração do threshold.
- A leitura dos ToF mostrar uma tendência coerente de ocupação ao longo do tempo. Não precisa ser exata em cada leitura individual, precisa ser consistente o suficiente para dar uma noção de enchimento.
- O dispositivo não apresentar resets frequentes ou perda de funcionalidade durante o período de teste.
- A bateria sustentar a operação durante o teste sem precisar de recarga intermediária.
- A dashboard refletir o status atualizado dentro de um intervalo razoável após o envio.

---

## Riscos e mitigações

Wi-Fi instável no local do coletor.
Mitigação: retry curto, medir RSSI no ponto de instalação e usar hotspot/rede dedicada durante o teste se necessário.

Consumo de bateria maior que o estimado.
Mitigação: encerrar Wi-Fi logo após o POST. Se autonomia for curta, aumentar intervalo para 60 min e revisar o circuito de alimentação/recarga.

Cross-talk no acrílico.
Mitigação: sensor encostado no acrílico + espuma preta. Se persistir, vedar com silicone direto sobre o sensor sem acrílico.

Falsos wakes do MPU.
Mitigação: calibrar threshold em campo. Se local com vibração constante, aumentar threshold.

Leitura dos ToF imprecisa com e-waste irregular.
Mitigação: mediana de 3 pontos. Margem de ±15% documentada como limitação. Para decisão logística, aceitável.
