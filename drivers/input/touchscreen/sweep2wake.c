/*
 * drivers/input/touchscreen/sweep2wake.c
 *
 *
 * Copyright (c) 2012, Dennis Rassmann <showp1984@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/input/sweep2wake.h>

/* Tuneables */
#define S2W_Y_LIMIT             2350
#define S2W_X_MAX               1540
#define S2W_X_B1                500
#define S2W_X_B2                1000
#define S2W_X_FINAL             300
#define S2W_PWRKEY_DUR          60

/* Resources */
unsigned int retry_cnt = 0;
bool s2w_switch = false;
bool s2w_error = false;
bool scr_suspended = false, exec_count = true;
bool barrier[2] = { false, false };

static struct input_dev *sweep2wake_pwrdev;
static DEFINE_MUTEX(pwrkeyworklock);

/* PowerKey setter */
void sweep2wake_setdev(struct input_dev *input_device)
{
	sweep2wake_pwrdev = input_device;
}

EXPORT_SYMBOL(sweep2wake_setdev);

/* PowerKey work func */
static void sweep2wake_presspwr(struct work_struct *sweep2wake_presspwr_work)
{
	if (!mutex_trylock(&pwrkeyworklock))
		return;

	input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 1);
	input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
	msleep(S2W_PWRKEY_DUR);
	input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 0);
	input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
	msleep(S2W_PWRKEY_DUR);
	mutex_unlock(&pwrkeyworklock);
}

static DECLARE_WORK(sweep2wake_presspwr_work, sweep2wake_presspwr);

/* PowerKey trigger */
void sweep2wake_pwrtrigger(void)
{
	schedule_work(&sweep2wake_presspwr_work);
}

/* Sweep2wake main function */
void detect_sweep2wake(int x, int y, struct lge_touch_data *ts)
{
	int prevx = 0, nextx = 0;

	if (!s2w_switch || !ts->ts_data.curr_data[0].state
	    || ts->ts_data.curr_data[1].state)
		return;

	/* left->right */
	if (scr_suspended) {
		prevx = 0;
		nextx = S2W_X_B1;
		if (barrier[0] || (x > prevx && x < nextx)) {
			prevx = nextx;
			nextx = S2W_X_B2;
			barrier[0] = true;
			if (barrier[1] || (x > prevx && x < nextx)) {
				prevx = nextx;
				barrier[1] = true;
				if (x > prevx && y > 0) {
					if (x > S2W_X_MAX - S2W_X_FINAL) {
						if (exec_count) {
							pr_info
							    ("[sweep2wake]: ON\n");
							sweep2wake_pwrtrigger();
							exec_count = false;
						}
					}
				}
			}
		}
	/* right->left */
	} else if (!scr_suspended) {
		prevx = S2W_X_MAX - S2W_X_FINAL;
		nextx = S2W_X_B2;
		if (barrier[0] || (x < prevx && x > nextx)) {
			prevx = nextx;
			nextx = S2W_X_B1;
			barrier[0] = true;
			if (barrier[1] || (x < prevx && x > nextx)) {
				prevx = nextx;
				barrier[1] = true;
				if (x < prevx) {
					if (x < S2W_X_FINAL && y > 2150) {
						if (exec_count) {
							pr_info
							    ("[sweep2wake]: OFF\n");
							sweep2wake_pwrtrigger();
							exec_count = false;
						}
					}
				}
			}
		}
	}
}

/*
 * INIT / EXIT stuff below here
 */

static int __init sweep2wake_init(void)
{
	pr_info("[sweep2wake]: %s done\n", __func__);
	return 0;
}

static void __exit sweep2wake_exit(void)
{
	return;
}

module_init(sweep2wake_init);
module_exit(sweep2wake_exit);

MODULE_DESCRIPTION("Sweep2wake");
MODULE_LICENSE("GPLv2");
