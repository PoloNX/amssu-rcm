/*
 * Fan driver for Nintendo Switch
 *
 * Copyright (c) 2018-2020 CTCaer
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <thermal/fan.h>
#include <power/regulator_5v.h>
#include <soc/gpio.h>
#include <soc/pinmux.h>
#include <soc/t210.h>
#include <utils/util.h>

void set_fan_duty(u32 duty)
{
	static bool fan_init = false;
	static u16 curr_duty = -1;

	if (curr_duty == duty)
		return;

	if (!fan_init)
	{
		// Fan tachometer.
		PINMUX_AUX(PINMUX_AUX_CAM1_PWDN) = PINMUX_TRISTATE | PINMUX_INPUT_ENABLE | PINMUX_PULL_UP | 1;
		gpio_config(GPIO_PORT_S, GPIO_PIN_7, GPIO_MODE_GPIO);
		gpio_output_enable(GPIO_PORT_S, GPIO_PIN_7, GPIO_OUTPUT_DISABLE);

		PWM(PWM_CONTROLLER_PWM_CSR_1) = PWM_CSR_EN | (0x100 << 16); // Max PWM to disable fan.

		PINMUX_AUX(PINMUX_AUX_LCD_GPIO2) = 1; // Set source to PWM1.
		gpio_config(GPIO_PORT_V, GPIO_PIN_4, GPIO_MODE_SPIO); // Fan power mode.

		fan_init = true;
	}

	if (duty > 236)
		duty = 236;

	// Inverted polarity.
	u32 inv_duty = 236 - duty;

	// If disabled send a 0 duty.
	if (inv_duty == 236)
	{
		PWM(PWM_CONTROLLER_PWM_CSR_1) = PWM_CSR_EN | (0x100 << 16); // Bit 24 is absolute 0%.
		regulator_5v_disable(REGULATOR_5V_FAN);

		// Disable fan.
		PINMUX_AUX(PINMUX_AUX_LCD_GPIO2) =
				PINMUX_INPUT_ENABLE | PINMUX_PARKED |  PINMUX_TRISTATE | PINMUX_PULL_DOWN; // Set source to PWM1.
	}
	else // Set PWM duty.
	{
		// Fan power supply.
		regulator_5v_enable(REGULATOR_5V_FAN);
		PWM(PWM_CONTROLLER_PWM_CSR_1) = PWM_CSR_EN | (inv_duty << 16);

		// Enable fan.
		PINMUX_AUX(PINMUX_AUX_LCD_GPIO2) = 1; // Set source to PWM1.
	}

	curr_duty = duty;
}

void get_fan_speed(u32 *duty, u32 *rpm)
{
	if (rpm)
	{
		u32  irq_count = 1;
		bool should_read = true;
		bool irq_val = 0;

		// Poll irqs for 2 seconds.
		int timer = get_tmr_us() + 1000000;
		while (timer - get_tmr_us())
		{
			irq_val = gpio_read(GPIO_PORT_S, GPIO_PIN_7);
			if (irq_val && should_read)
			{
				irq_count++;
				should_read = false;
			}
			else if (!irq_val)
				should_read = true;
		}

		// Calculate rpm based on triggered interrupts.
		*rpm = 60000000 / ((1000000 * 2) / irq_count);
	}

	if (duty)
		*duty = 236 - ((PWM(PWM_CONTROLLER_PWM_CSR_1) >> 16) & 0xFF);
}
