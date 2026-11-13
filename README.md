# Simulação em C de Hardware de Telecomunicação

**Universidade Federal de Roraima (UFRR) · Departamento de Ciência da Computação (DCC)**
**Disciplina:** Arquitetura e Organização de Computadores (AOC)
**Professor:** Herbert Oliveira Rocha
**Atividade:** Atividade Prática — Simulação em C de Hardware de Telecomunicação (ponto extra)
**Dupla:** Mateus Henrique e Rafael da Silva
**Data:** 12/11/2026

---

## Sobre a atividade

O programa `modem_sim.c` simula, em um único processo C, quatro componentes de um sistema de comunicação:

| Componente | Papel na simulação |
|---|---|
| **CPU (PC)** | Executa o software. Só acessa o hardware pelos registradores da controladora, via barramento. |
| **Controladora de E/S** | Registradores `STATUS`, `CTRL` e `DATA` e sinais `CS`, `RD`, `WR`, `INT`, `INTA` e `DADOS`. |
| **Modem** | Modula e demodula bits por amplitude (ASK), frequência (FSK) ou fase (PSK). |
| **Linha telefônica** | Meio físico com ruído: com probabilidade `p_ruido`, inverte o símbolo transmitido. |

A mensagem transmitida é `01001010000100` (14 bits), enviada bit a bit, seguida de um bit de paridade par. A linha entrega **1 bit a cada 8 instruções** da CPU (`PERIODO = 8`). Essa diferença de velocidade é o que justifica o uso de interrupções.

## Estrutura do repositório

```
.
├── modem_sim.c                     # código-fonte com as tarefas implementadas
├── Tabela_teste/
│   └── Atividade_Pratica_Preenchida (1).pdf   # tabelas A, B e C preenchidas
├── .gitignore                      # ignora executáveis (*.exe)
└── README.md
```

## Como compilar e executar

Requisito: compilador C (`gcc` ou `clang`). No Windows, o `gcc` vem com o MinGW-w64.

```bash
gcc -Wall -o modem_sim modem_sim.c
./modem_sim <modo> <modulacao> <p_ruido>
```

| Argumento | Valores |
|---|---|
| `modo` | `poll` (E/S programada) ou `irq` (E/S por interrupção) |
| `modulacao` | `ask`, `fsk` ou `psk` |
| `p_ruido` | probabilidade de inverter cada símbolo, de `0` a `1` |

Exemplos:

```bash
./modem_sim poll ask 0      # polling, sem ruído
./modem_sim irq fsk 0.05    # interrupção, FSK, 5% de ruído
```

No PowerShell, use `.\modem_sim.exe` no lugar de `./modem_sim`.

O programa usa `srand(42)`, então **os resultados são reproduzíveis**: as mesmas entradas sempre geram as mesmas saídas.

## O que foi implementado

### Tarefa 2 — Modulação FSK e PSK (`modular` e `demodular`)

Seguindo o padrão da ASK já fornecida:

| Modulação | Bit 1 | Bit 0 | Regra de demodulação |
|---|---|---|---|
| ASK (fornecida) | amplitude 2.0 | amplitude 1.0 | `amp > 1.5` |
| **FSK** | frequência 2.0 | frequência 1.0 | `freq > 1.5` |
| **PSK** | fase 180° | fase 0° | `fase > 90` |

Em todos os casos a decisão usa o ponto médio entre os dois símbolos.

### Tarefa 3 — E/S programada (polling) e rotina de interrupção

- **Polling** (laço de `simular`): a cada instrução, a CPU lê `STATUS` com `cpu_le(0)`. Se `RX_READY` estiver ligado, lê `DATA` com `cpu_le(1)` e guarda o bit. Se não estiver, incrementa `ciclos_espera` (busy-wait).
- **Interrupção** (`isr`): a CPU executa outro programa (`trabalho_util++`) e só é desviada quando a controladora levanta `INT`. A ISR ativa `INTA`, lê `DATA` pelo barramento, guarda o bit e baixa `INT` e `INTA`.

Nos dois modos, a CPU acessa o hardware **somente pelo barramento** (`cpu_le`). Ela nunca lê `ctrl.DATA` diretamente.

### Tarefa 4 — Bit de paridade e detecção de erro

- **No transmissor:** a paridade par é o XOR dos 14 bits da mensagem. Como `01001010000100` tem quatro bits 1, a paridade vale **0**. Ela é enviada como **15º bit**, pela mesma linha ruidosa.
- **No receptor** (`relatorio`): calcula o XOR dos 15 bits recebidos. Se o resultado for 1, liga `ST_ERR` na controladora.
- `simular` e `relatorio` passaram a receber `N_BITS + 1`.

## Resultados

Valores obtidos com o código deste repositório. As tabelas completas estão no PDF em `Tabela_teste/`.

### Tabela A — Resumo

| Teste | Modo | Modulação | p_ruido | Bits errados (de 14) | ERR | Ciclos esperando | Trabalho útil |
|---|---|---|---|---|---|---|---|
| T1–T4 | poll | ASK / FSK / PSK | 0 | 0 | não | 98 | 0 |
| T5 | irq | ASK | 0 | 0 | não | 0 | 113 |
| T6–T8 | irq | ASK / FSK / PSK | 0,05 | 2 | **não** | 0 | 113 |
| T9–T11 | irq | ASK / FSK / PSK | 0,2 | 3 | **não** | 0 | 113 |
| T12 (opc.) | poll | ASK | 0 | 0 | não | 14 (PERIODO = 2) / 434 (PERIODO = 32) | 0 |

Para a T12, altere `#define PERIODO 8` no código e compile de novo.

### Testes extras da dupla (Tabela C)

| Teste | Comando | Resultado | Conclusão |
|---|---|---|---|
| Erro simples | `./modem_sim irq ask 0.03` | 1 bit errado, ERR = sim | A paridade detecta número ímpar de erros |
| Dois erros | `./modem_sim irq ask 0.05` | 2 bits errados, ERR = não | Número par de erros passa despercebido |
| Ruído total | `./modem_sim irq ask 1.0` | 14 errados + paridade invertida (15 inversões), ERR = sim | 15 é ímpar, então o erro é detectado |

## Análise

**Polling × interrupção.** Com `PERIODO = 8`, a recepção dos 15 bits leva 113 instruções. No polling, **98 delas (cerca de 87%) são espera ocupada**: a CPU só pergunta "já chegou?". Com interrupção, as 113 instruções ficam livres para outro programa, e a CPU só é desviada quando `INT` sobe. A simulação não contabiliza o custo da ISR. Num sistema real, cada interrupção paga a troca de contexto (empilhar PC e PSW, salvar e restaurar registradores, `IRET`). Por isso a interrupção bit a bit não serve para dispositivos rápidos, e para eles se usa **DMA**.

**Efeito do PERIODO.** A espera no polling é de cerca de `PERIODO − 1` ciclos por bit: 14 ciclos no total com PERIODO = 2 e 434 com PERIODO = 32. Quanto mais lento o periférico em relação à CPU, maior o desperdício do polling.

**Por que ASK, FSK e PSK deram resultados idênticos.** A função `linha()` inverte o símbolo inteiro (amplitude, frequência e fase) com a mesma probabilidade, e a semente é fixa. Assim, os mesmos bits são corrompidos em qualquer modulação. Numa linha real com ruído aditivo, FSK e PSK costumam ser mais robustas que ASK, porque o ruído afeta principalmente a amplitude.

**Limite da paridade.** A paridade par só detecta **número ímpar** de inversões. Em T6–T8 houve 2 erros. Em T9–T11 houve 4 inversões: 3 nos dados mais o próprio bit de paridade. Nos dois casos o XOR voltou a 0 e o `ST_ERR` não foi ligado, mesmo com a mensagem corrompida. Para detectar erros múltiplos e em rajada se usam códigos mais fortes, como CRC e checksum. Para corrigir erros, usam-se códigos como Hamming.

## Sequência de sinais no barramento (Tabela B)

| Operação | Evento | CS | RD | WR | INT | INTA |
|---|---|---|---|---|---|---|
| Configuração | WR CTRL | 1 | 0 | 1 | 0 | 0 |
| Polling | RD STATUS / RD DATA | 1 | 1 | 0 | 0 | 0 |
| Interrupção | IRQ (controladora) | 0 | 0 | 0 | 1 | 0 |
| Interrupção | INTA (CPU) | 0 | 0 | 0 | 1 | 1 |
| Interrupção | RD DATA dentro da ISR | 1 | 1 | 0 | 1 | 1 |
| Interrupção | Fim da ISR | 0 | 0 | 0 | 0 | 0 |

Os valores de `DADOS` em cada passo estão no PDF.
