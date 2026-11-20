/**
 * @file display.h
 * @brief Tela do terminal de pagamento (ST7789V 240x320, RGB565, SPI1).
 *
 * As funções só enfileiram o pedido e retornam na hora: o desenho e os atrasos
 * da inicialização rodam numa thread própria, de prioridade menor que a da
 * máquina de estados, para nunca atrasar o trabalho de segurança das faixas.
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdint.h>

/**
 * @brief Inicia a thread do display, que faz o reset por hardware e a
 *        sequência SWRESET, SLPOUT, COLMOD 55, MADCTL 00, DISPON.
 *
 * @return 0 em sucesso ou errno negativo.
 */
int display_init(void);

/**
 * @brief Limpa a tela e desenha a placa reconhecida.
 *
 * @param plate Placa de 7 caracteres.
 * @return 0 se o pedido foi enfileirado, -EINVAL ou -ENOMEM.
 */
int display_show_plate(const char *plate);

/**
 * @brief Desenha "BRL <reais>.<centavos>", "SIMULATED PAYMENT" e "NOT PIX".
 *
 * @param amount Valor em centavos.
 * @return 0 se o pedido foi enfileirado ou -ENOMEM.
 */
int display_show_charge(uint32_t amount);

/**
 * @brief Apaga a tela.
 *
 * @return 0 se o pedido foi enfileirado ou -ENOMEM.
 */
int display_clear(void);

#endif /* DISPLAY_H_ */
