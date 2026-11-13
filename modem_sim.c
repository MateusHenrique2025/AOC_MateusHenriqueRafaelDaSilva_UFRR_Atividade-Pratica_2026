#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MSG "01001010000100" /* mensagem da aula */
#define N_BITS 14
#define PERIODO 8 /* a linha entrega 1 bit a cada 8 instrucoes da CPU */

/* ---------- bits de STATUS e CTRL ---------- */
#define ST_RX_READY 0x01
#define ST_TX_EMPTY 0x02
#define ST_ERR 0x04
#define CT_IRQ_EN 0x01
#define CT_MOD_MASK 0x30
#define CT_MOD_ASK 0x00
#define CT_MOD_FSK 0x10
#define CT_MOD_PSK 0x20

/* ---------- sinais do barramento ---------- */
typedef struct { int CS, RD, WR, INT, INTA; uint8_t DADOS; } Barramento;

/* ---------- controladora ---------- */
typedef struct {
    uint8_t STATUS, CTRL, DATA;
    int recebido[N_BITS + 1]; /* +1 para o bit de paridade */
    int n_rx;
} Controladora;

/* ---------- sinal fisico na linha ---------- */
typedef struct { double amp, freq, fase; } Amostra;

Barramento bus;
Controladora ctrl;
double p_ruido = 0.0;
long ciclos_espera = 0; /* instrucoes gastas esperando (polling) */
long trabalho_util = 0; /* instrucoes de outro programa (modo irq) */
int traco = 1; /* 1 = imprime os sinais a cada acesso */

void imprime_sinais(const char *evento) {
    if (traco)
        printf("[%-12s] CS=%d RD=%d WR=%d INT=%d INTA=%d DADOS=0x%02X\n",
               evento, bus.CS, bus.RD, bus.WR, bus.INT, bus.INTA, bus.DADOS);
}

/* ============ MODEM (hardware) ============ */
Amostra modular(int bit, uint8_t modo) {
    Amostra a = {1.0, 1.0, 0.0}; /* portadora: amp 1, freq 1, fase 0 */
    switch (modo) {
        case CT_MOD_ASK: a.amp = bit ? 2.0 : 1.0; break;
        case CT_MOD_FSK: a.freq = bit ? 2.0 : 1.0; break;
        case CT_MOD_PSK: a.fase = bit ? 180.0 : 0.0; break;
    }
    return a;
}

int demodular(Amostra a, uint8_t modo) {
    switch (modo) {
        case CT_MOD_ASK: return a.amp > 1.5;
        case CT_MOD_FSK: return a.freq > 1.5;
        case CT_MOD_PSK: return a.fase > 90.0;
    }
    return 0;
}

/* ============ LINHA TELEFONICA (meio fisico) ============ */
Amostra linha(Amostra a) {
    if ((double)rand() / RAND_MAX < p_ruido) { /* ruido inverte o simbolo */
        a.amp = 3.0 - a.amp;
        a.freq = 3.0 - a.freq;
        a.fase = 180.0 - a.fase;
    }
    return a;
}

/* ============ CONTROLADORA ============ */
void ctrl_chega_bit(Amostra a) {
    ctrl.DATA = (uint8_t)demodular(a, ctrl.CTRL & CT_MOD_MASK);
    ctrl.STATUS |= ST_RX_READY;
    if (ctrl.CTRL & CT_IRQ_EN) { bus.INT = 1; imprime_sinais("IRQ"); }
}

/* ============ CPU: acesso ao barramento ============ */
uint8_t cpu_le(int reg) { /* reg 0 = STATUS, 1 = DATA */
    bus.CS = 1; bus.RD = 1; bus.WR = 0;
    bus.DADOS = reg ? ctrl.DATA : ctrl.STATUS;
    imprime_sinais(reg ? "RD DATA" : "RD STATUS");
    if (reg) ctrl.STATUS &= ~ST_RX_READY; /* leitura de DATA limpa RX_READY */
    bus.CS = 0; bus.RD = 0;
    return bus.DADOS;
}

void cpu_escreve(int reg, uint8_t v) { /* reg 0 = CTRL, 1 = DATA */
    bus.CS = 1; bus.WR = 1; bus.RD = 0; bus.DADOS = v;
    imprime_sinais(reg ? "WR DATA" : "WR CTRL");
    if (reg) ctrl.DATA = v; else ctrl.CTRL = v;
    bus.CS = 0; bus.WR = 0;
}

/* ============ CPU: rotina de interrupcao ============ */
void isr(void) {
    bus.INTA = 1; imprime_sinais("INTA");
    ctrl.recebido[ctrl.n_rx++] = cpu_le(1);
    bus.INT = 0; bus.INTA = 0;
}

/* ============ SIMULACAO ============ */
void simular(int modo_irq, uint8_t modulacao, int total_bits) {
    memset(&ctrl, 0, sizeof ctrl); memset(&bus, 0, sizeof bus);
    ciclos_espera = trabalho_util = 0;
    cpu_escreve(0, modulacao | (modo_irq ? CT_IRQ_EN : 0)); /* configura CTRL */
    int i = 0; long ciclo = 0;

    /* Tarefa 4: calculo da paridade par da mensagem original */
    int paridade = 0;
    for (int j = 0; j < N_BITS; j++) paridade ^= (MSG[j] - '0');

    while (ctrl.n_rx < total_bits) {
        /* hardware: a cada PERIODO ciclos chega um bit pela linha */
        if (ciclo % PERIODO == 0 && i < total_bits) {
            /* Tarefa 4: o 15o bit (i == N_BITS) transmite a paridade */
            int bit = (i < N_BITS) ? MSG[i] - '0' : paridade;
            ctrl_chega_bit(linha(modular(bit, modulacao)));
            i++;
        }
        /* CPU executa UMA instrucao (fluxograma da aula) */
        if (modo_irq) {
            trabalho_util++; /* outro programa roda */
            if (bus.INT) isr(); /* "Ha interrupcao?" */
        } else {
            /* Tarefa 3: polling - ler STATUS; se RX_READY, ler DATA */
            uint8_t status = cpu_le(0);
            if (status & ST_RX_READY) {
                ctrl.recebido[ctrl.n_rx++] = cpu_le(1);
            } else {
                ciclos_espera++;
            }
        }
        ciclo++;
        if (ciclo > 100000) { puts("TIMEOUT: a CPU nunca recebeu todos os bits"); break; }
    }
}

void relatorio(int total_bits) {
    int erros = 0;
    printf("\nEnviado : %s\nRecebido: ", MSG);
    for (int k = 0; k < N_BITS; k++) {
        printf("%d", ctrl.recebido[k]);
        if (ctrl.recebido[k] != MSG[k] - '0') erros++;
    }
    printf("\nBits errados: %d de %d\n", erros, N_BITS);

    /* Tarefa 4: verificar paridade e ligar ST_ERR */
    int paridade_recebida = ctrl.recebido[N_BITS]; /* Pega o 15o bit */
    int paridade_calc = 0;
    
    /* Calcula o XOR dos 14 bits de dados recebidos */
    for (int k = 0; k < N_BITS; k++) {
        paridade_calc ^= ctrl.recebido[k];
    }
    
    /* O XOR de todos os 15 bits (dados + paridade) deve ser ZERO */
    int xor_total = paridade_calc ^ paridade_recebida;
    
    if (xor_total != 0) {
        ctrl.STATUS |= ST_ERR;
    } else {
        ctrl.STATUS &= ~ST_ERR;
    }

    printf("ERR na controladora: %s\n", (ctrl.STATUS & ST_ERR) ? "sim" : "nao");
    printf("Ciclos esperando (polling): %ld\nTrabalho util (irq): %ld\n",
           ciclos_espera, trabalho_util);
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "uso: %s poll|irq ask|fsk|psk p_ruido\n", argv[0]); return 1; }
    int modo_irq = strcmp(argv[1], "irq") == 0;
    uint8_t modulacao = !strcmp(argv[2], "fsk") ? CT_MOD_FSK :
                        !strcmp(argv[2], "psk") ? CT_MOD_PSK : CT_MOD_ASK;
    p_ruido = atof(argv[3]);
    srand(42); /* resultados reproduziveis */
    simular(modo_irq, modulacao, N_BITS + 1);
    relatorio(N_BITS + 1);
    return 0;
}