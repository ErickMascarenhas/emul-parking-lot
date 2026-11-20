/**
 * @file lane.c
 * @brief Máquina de estados das faixas.
 */

#include "lane.h"

#include <stdbool.h>

#include "camera.h"
#include "display.h"
#include "distance.h"
#include "line.h"
#include "payment.h"
#include "session.h"
#include "tariff.h"

/** @brief Estado de uma faixa. */
struct lane {
	enum hw_lane id;                  /**< Qual faixa. */
	enum lane_state state;            /**< Estado atual. */
	int64_t since_ms;                 /**< Início do prazo do estado atual. */
	int64_t buzzer_off_ms;            /**< Quando desligar o buzzer; 0 se desligado. */
	int io_error;                     /**< Último erro de escrita numa saída. */
	struct line_framer camera;        /**< Montador de quadros da câmera. */
	uint32_t capture_id;              /**< ID da última captura pedida. */
	unsigned int attempts;            /**< Tentativas de captura deste veículo. */
	char plate[CAMERA_PLATE_LEN + 1]; /**< Placa aceita. */
};

static struct lane lanes[HW_LANE_COUNT];

/** @brief Muda de estado e reinicia o prazo. */
static void enter(struct lane *l, enum lane_state state, int64_t now_ms)
{
	l->state = state;
	l->since_ms = now_ms;
}

/** @brief Indica se o prazo do estado atual venceu. */
static bool expired(const struct lane *l, int64_t now_ms, int64_t budget_ms)
{
	return now_ms - l->since_ms >= budget_ms;
}

/** @brief Escreve uma saída; um erro fica guardado e trava a faixa no próximo passo. */
static int drive(struct lane *l, enum hw_output output, bool on)
{
	int err = hw_output_set(l->id, output, on);

	if (err < 0) {
		l->io_error = err;
	}
	return err;
}

/** @brief Há amostra de distância atual abaixo do limiar de chegada. */
static bool vehicle_present(const struct lane *l, int64_t now_ms)
{
	uint16_t raw;

	return distance_get(l->id, now_ms, &raw) == 0 && raw < DISTANCE_ARRIVAL_CODE;
}

/** @brief Há amostra de distância atual acima do limiar de saída. */
static bool vehicle_gone(const struct lane *l, int64_t now_ms)
{
	uint16_t raw;

	return distance_get(l->id, now_ms, &raw) == 0 && raw > DISTANCE_CLEAR_CODE;
}

/**
 * @brief Para o motor e pede assistência.
 *
 * Desliga o motor antes de tudo, apaga o verde, acende o vermelho e pulsa o
 * buzzer uma vez. Na saída, invalida a cobrança antes de cancelá-la.
 *
 * @param latched true para travar até o reset; false para assistência com a
 *        cancela fechada, que rearma quando o veículo vai embora.
 */
static void stop(struct lane *l, int64_t now_ms, bool latched)
{
	bool pulse = l->state != LANE_ASSIST;

	if (l->state == LANE_FAULT) {
		return;
	}
	if (drive(l, HW_OUT_MOTOR_ENABLE, false) < 0 || drive(l, HW_OUT_GREEN, false) < 0 ||
	    drive(l, HW_OUT_RED, true) < 0) {
		latched = true;
	}
	if (pulse && drive(l, HW_OUT_BUZZER, true) == 0) {
		l->buzzer_off_ms = now_ms + LANE_BUZZER_PULSE_MS;
	}
	if (l->id == HW_LANE_EXIT && payment_abort() < 0) {
		latched = true;
	}
	enter(l, latched ? LANE_FAULT : LANE_ASSIST, now_ms);
}

/** @brief Desliga o buzzer quando o pulso termina. */
static void service_buzzer(struct lane *l, int64_t now_ms)
{
	if (l->buzzer_off_ms != 0 && now_ms >= l->buzzer_off_ms &&
	    drive(l, HW_OUT_BUZZER, false) == 0) {
		l->buzzer_off_ms = 0;
	}
}

/** @brief Lê os bytes da câmera e devolve o próximo quadro completo, ou NULL. */
static const char *camera_rx(struct lane *l, int64_t now_ms)
{
	enum hw_uart uart = l->id == HW_LANE_ENTRY ? HW_UART_CAMERA_ENTRY : HW_UART_CAMERA_EXIT;
	uint8_t byte;

	while (hw_uart_read(uart, &byte) == 0) {
		if (line_feed(&l->camera, byte, now_ms) == LINE_READY) {
			return line_frame(&l->camera);
		}
	}
	return NULL;
}

/** @brief Pede a próxima captura, com ID novo, ou a assistência se acabaram. */
static void next_attempt(struct lane *l, int64_t now_ms)
{
	enum hw_uart uart = l->id == HW_LANE_ENTRY ? HW_UART_CAMERA_ENTRY : HW_UART_CAMERA_EXIT;

	if (l->attempts == LANE_CAPTURE_ATTEMPTS) {
		stop(l, now_ms, false);
		return;
	}
	l->attempts++;
	l->capture_id++;
	if (camera_request(uart, l->capture_id) < 0) {
		stop(l, now_ms, false);
		return;
	}
	enter(l, LANE_IDENTIFYING, now_ms);
}

/** @brief Liga o motor no sentido de abrir. O vermelho segue aceso. */
static void open_barrier(struct lane *l, int64_t now_ms)
{
	if (drive(l, HW_OUT_MOTOR_DIRECTION, true) < 0 || drive(l, HW_OUT_MOTOR_ENABLE, true) < 0) {
		stop(l, now_ms, true);
		return;
	}
	enter(l, LANE_OPENING, now_ms);
}

/**
 * @brief Decide o que fazer com uma placa aceita.
 *
 * Entrada: abre se a placa não está estacionada e há vaga. Saída: só uma placa
 * com sessão paga; o valor é congelado aqui, depois de desenhar a placa e
 * antes de desenhar o valor e enviar a cobrança.
 */
static void accept_plate(struct lane *l, int64_t now_ms)
{
	int64_t entry_ms;
	uint32_t amount;

	if (l->id == HW_LANE_ENTRY) {
		if (session_find(l->plate, NULL) == 0 ||
		    session_count() >= CONFIG_PARKING_CAPACITY) {
			stop(l, now_ms, false);
		} else {
			open_barrier(l, now_ms);
		}
		return;
	}
	if (session_find(l->plate, &entry_ms) < 0 || display_show_plate(l->plate) < 0 ||
	    tariff_compute(now_ms - entry_ms, CONFIG_PARKING_TARIFF_UNIT_MS, &amount) < 0 ||
	    display_show_charge(amount) < 0 || payment_start(l->plate, amount, now_ms) < 0) {
		stop(l, now_ms, false);
		return;
	}
	enter(l, LANE_PAYING, now_ms);
}

/** @brief Trata a identificação: abandono, resultado da câmera e prazo. */
static void identify(struct lane *l, const char *frame, int64_t now_ms)
{
	uint16_t raw;
	unsigned int confidence;

	if (vehicle_gone(l, now_ms)) {
		enter(l, LANE_IDLE, now_ms);
		return;
	}
	/* Um resultado só autoriza com amostra de distância atual. */
	if (frame != NULL && distance_get(l->id, now_ms, &raw) == 0 &&
	    camera_parse_plate(frame, l->capture_id, l->plate, &confidence) == 0) {
		if (confidence >= CAMERA_MIN_CONFIDENCE) {
			accept_plate(l, now_ms);
		} else {
			next_attempt(l, now_ms);
		}
		return;
	}
	if (expired(l, now_ms, LANE_CAPTURE_TIMEOUT_MS)) {
		next_attempt(l, now_ms);
	}
}

/** @brief No fim de curso superior: motor desligado antes de acender o verde. */
static void opened(struct lane *l, int64_t now_ms)
{
	if (drive(l, HW_OUT_MOTOR_ENABLE, false) < 0 || drive(l, HW_OUT_RED, false) < 0 ||
	    drive(l, HW_OUT_GREEN, true) < 0) {
		stop(l, now_ms, true);
		return;
	}
	enter(l, LANE_OPEN, now_ms);
}

/**
 * @brief Acompanha a passagem: feixe ativo, depois livre com o veículo longe.
 *
 * Só a passagem completa abre (entrada) ou encerra (saída) a sessão. O prazo
 * conta desde a abertura e vale também com o feixe interrompido.
 */
static void passage(struct lane *l, int beam, int64_t now_ms)
{
	int err;

	if (beam) {
		l->state = LANE_PASSING;
	} else if (l->state == LANE_PASSING && vehicle_gone(l, now_ms)) {
		err = l->id == HW_LANE_ENTRY ? session_add(l->plate, now_ms)
					     : session_remove(l->plate);
		if (err < 0 || drive(l, HW_OUT_GREEN, false) < 0 || drive(l, HW_OUT_RED, true) < 0 ||
		    drive(l, HW_OUT_MOTOR_DIRECTION, false) < 0 ||
		    drive(l, HW_OUT_MOTOR_ENABLE, true) < 0) {
			stop(l, now_ms, true);
			return;
		}
		enter(l, LANE_CLOSING, now_ms);
		return;
	}
	if (expired(l, now_ms, LANE_PASSAGE_BUDGET_MS)) {
		stop(l, now_ms, true);
	}
}

/** @brief Acompanha o fechamento; obstrução para o motor, sem reverter. */
static void closing(struct lane *l, int beam, int lower, int64_t now_ms)
{
	if (beam || expired(l, now_ms, LANE_MOTOR_BUDGET_MS)) {
		stop(l, now_ms, true);
	} else if (lower) {
		if (drive(l, HW_OUT_MOTOR_ENABLE, false) < 0) {
			stop(l, now_ms, true);
			return;
		}
		enter(l, LANE_IDLE, now_ms);
	}
}

void lane_init(enum hw_lane lane, int64_t now_ms)
{
	struct lane *l = &lanes[lane];

	*l = (struct lane){.id = lane};
	line_init(&l->camera, true);
	enter(l, LANE_BOOT, now_ms);
}

void lane_fail(enum hw_lane lane, int64_t now_ms)
{
	stop(&lanes[lane], now_ms, true);
}

void lane_step(enum hw_lane lane, int64_t now_ms)
{
	struct lane *l = &lanes[lane];
	const char *frame = camera_rx(l, now_ms);
	enum payment_event paid = lane == HW_LANE_EXIT ? payment_poll(now_ms) : PAYMENT_EV_NONE;
	int upper;
	int lower;
	int beam;

	service_buzzer(l, now_ms);
	if (l->state == LANE_BOOT) {
		if (expired(l, now_ms, LANE_SETTLE_MS)) {
			enter(l, LANE_IDLE, now_ms);
		}
		return;
	}
	upper = hw_input_get(lane, HW_IN_LIMIT_UPPER);
	lower = hw_input_get(lane, HW_IN_LIMIT_LOWER);
	beam = hw_input_get(lane, HW_IN_BEAM);
	if (upper < 0 || lower < 0 || beam < 0 || l->io_error < 0 || distance_faulted() ||
	    (upper && lower)) {
		stop(l, now_ms, true);
		return;
	}

	switch (l->state) {
	case LANE_IDLE:
		if (!lower) {
			stop(l, now_ms, true);
		} else if (vehicle_present(l, now_ms)) {
			l->attempts = 0;
			next_attempt(l, now_ms);
		}
		break;
	case LANE_IDENTIFYING:
		identify(l, frame, now_ms);
		break;
	case LANE_PAYING:
		if (vehicle_gone(l, now_ms) || paid == PAYMENT_EV_FAILED) {
			stop(l, now_ms, false);
		} else if (paid == PAYMENT_EV_PAID) {
			open_barrier(l, now_ms);
		}
		break;
	case LANE_OPENING:
		if (upper) {
			opened(l, now_ms);
		} else if (expired(l, now_ms, LANE_MOTOR_BUDGET_MS)) {
			stop(l, now_ms, true);
		}
		break;
	case LANE_OPEN:
	case LANE_PASSING:
		passage(l, beam, now_ms);
		break;
	case LANE_CLOSING:
		closing(l, beam, lower, now_ms);
		break;
	case LANE_ASSIST:
		if (!lower) {
			stop(l, now_ms, true);
		} else if (!beam && vehicle_gone(l, now_ms)) {
			enter(l, LANE_IDLE, now_ms);
		}
		break;
	case LANE_BOOT:
	case LANE_FAULT:
		break;
	}
}

enum lane_state lane_state_get(enum hw_lane lane)
{
	return lanes[lane].state;
}
