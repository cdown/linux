// SPDX-License-Identifier: GPL-2.0-only
/*
 * sysctl.c: General linux system control interface
 */

#include <linux/sysctl.h>
#include <linux/printk.h>
#include <linux/capability.h>
#include <linux/ratelimit.h>
#include <linux/console.h>
#include "internal.h"

static const int ten_thousand = 10000;
static const int min_loglevel = LOGLEVEL_EMERG;
static const int max_loglevel = LOGLEVEL_DEBUG;

static int proc_dointvec_minmax_sysadmin(struct ctl_table *table, int write,
				void *buffer, size_t *lenp, loff_t *ppos)
{
	if (write && !capable(CAP_SYS_ADMIN))
		return -EPERM;

	return proc_dointvec_minmax(table, write, buffer, lenp, ppos);
}

static int printk_sysctl_deprecated(struct ctl_table *table, int write,
				    void __user *buffer, size_t *lenp,
				    loff_t *ppos)
{
	int res = proc_dointvec(table, write, buffer, lenp, ppos);

	if (write) {
		pr_warn_ratelimited(
			"printk: The kernel.printk sysctl is deprecated and will be removed soon. Use kernel.force_console_loglevel, kernel.default_message_loglevel, kernel.minimum_console_loglevel, or kernel.default_console_loglevel instead.\n"
		);

		/*
		 * If you specify kernel.printk, you're using the old API, and
		 * we should just force all consoles to the value specified.
		 * This is how it was before per-console levels were
		 * implemented.
		 */
		console_force_loglevel();
	}

	return res;
}

static int printk_force_console_loglevel(struct ctl_table *table, int write,
					 void __user *buffer, size_t *lenp,
					 loff_t *ppos)
{
	int res = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (write)
		console_force_loglevel();
	return res;
}

static struct ctl_table printk_sysctls[] = {
	{
		.procname	= "printk",
		.data		= &console_loglevel,
		.maxlen		= 4*sizeof(int),
		.mode		= 0644,
		.proc_handler	= printk_sysctl_deprecated,
	},
	{
		.procname	= "printk_ratelimit",
		.data		= &printk_ratelimit_state.interval,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_jiffies,
	},
	{
		.procname	= "printk_ratelimit_burst",
		.data		= &printk_ratelimit_state.burst,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "printk_delay",
		.data		= &printk_delay_msec,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= (void *)&ten_thousand,
	},
	{
		.procname	= "printk_devkmsg",
		.data		= devkmsg_log_str,
		.maxlen		= DEVKMSG_STR_MAX_SIZE,
		.mode		= 0644,
		.proc_handler	= devkmsg_sysctl_set_loglvl,
	},
	{
		.procname	= "dmesg_restrict",
		.data		= &dmesg_restrict,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax_sysadmin,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "kptr_restrict",
		.data		= &kptr_restrict,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax_sysadmin,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_TWO,
	},
	{
		.procname	= "force_console_loglevel",
		.data		= &console_loglevel,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= printk_force_console_loglevel,
		.extra1		= (void *)&min_loglevel,
		.extra2		= (void *)&max_loglevel,
	},
	{
		.procname	= "default_message_loglevel",
		.data		= &default_message_loglevel,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= (void *)&min_loglevel,
		.extra2		= (void *)&max_loglevel,
	},
	{
		.procname	= "default_console_loglevel",
		.data		= &default_console_loglevel,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= (void *)&min_loglevel,
		.extra2		= (void *)&max_loglevel,
	},
	{
		.procname	= "minimum_console_loglevel",
		.data		= &minimum_console_loglevel,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= (void *)&min_loglevel,
		.extra2		= (void *)&max_loglevel,
	},
	{}
};

void __init printk_sysctl_init(void)
{
	register_sysctl_init("kernel", printk_sysctls);
}
