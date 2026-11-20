/**
 * @file main.c
 * @brief Inicialização e laço de atendimento das duas faixas.
 *
 * O laço roda a cada 1 ms de tempo virtual e nunca bloqueia. As bordas dos fins
 * de curso e dos feixes chegam pela work queue do sistema e atendem a faixa na
 * hora; o mutex impede que work queue e laço mexam na mesma faixa ao mesmo tempo.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "display.h"
#include "distance.h"
#include "hw.h"
#include "lane.h"
#include "payment.h"

#define LOOP_PERIOD_MS 1

static K_MUTEX_DEFINE(lock);

/** @brief Borda de entrada, em contexto de work queue: atende a faixa já. */
static void on_input(enum hw_lane lane, enum hw_input input)
{
	ARG_UNUSED(input);

	if (k_mutex_lock(&lock, K_FOREVER) == 0) {
		lane_step(lane, k_uptime_get());
		k_mutex_unlock(&lock);
	}
}

int main(void)
{
	int64_t now = k_uptime_get();
	int err = hw_init();

	if (err < 0) {
		printk("hw_init falhou: %d\n", err);
		return 0;
	}
	lane_init(HW_LANE_ENTRY, now);
	lane_init(HW_LANE_EXIT, now);
	err = payment_init();
	if (err == 0) {
		err = display_init();
	}
	if (err < 0) {
		printk("terminal de pagamento indisponível: %d\n", err);
		lane_fail(HW_LANE_EXIT, now);
	}
	hw_set_input_callback(on_input);

	while (true) {
		if (k_mutex_lock(&lock, K_FOREVER) == 0) {
			now = k_uptime_get();
			distance_service(now);
			lane_step(HW_LANE_ENTRY, now);
			lane_step(HW_LANE_EXIT, now);
			k_mutex_unlock(&lock);
		}
		k_sleep(K_MSEC(LOOP_PERIOD_MS));
	}
	return 0;
}
