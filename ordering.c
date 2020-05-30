// SPDX-License-Identifier: GPL-3.0
/*
 * ordering.c
 *
 * Here's a sample kernel module showing the memory reordering.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/smpboot.h>
#include <linux/percpu.h>
#include <linux/semaphore.h>
#include <linux/delay.h>

/* #define CONFIG_ORDERING_RMO */

static DEFINE_PER_CPU(struct task_struct *, ordering_tasks);

#ifdef CONFIG_ORDERING_RMO
static atomic_t count = ATOMIC_INIT(0);
static unsigned int a, b;

static void ordering_thread_fn_cpu0(void)
{
	atomic_inc(&count);
}

static void ordering_thread_fn_cpu1(void)
{
	int temp = atomic_read(&count);

	a = temp;
#ifdef CONFIG_USE_CPU_BARRIER
	smp_wmb();
#else
	/* Prevent compiler reordering. */
	barrier();
#endif
	b = temp;
}

static void ordering_thread_fn_cpu2(void)
{
	unsigned int c, d;

	d = b;
#ifdef CONFIG_USE_CPU_BARRIER
	smp_rmb();
#else
	/* Prevent compiler reordering. */
	barrier();
#endif
	c = a;

	if ((int)(d - c) > 0)
		pr_info("reorders detected, a = %d, b = %d\n", c, d);
}
#else
static int x, y;
static int r1, r2;

static struct semaphore sem_x;
static struct semaphore sem_y;
static struct semaphore sem_end;

static void ordering_thread_fn_cpu0(void)
{
	down(&sem_x);
	x = 1;
#ifdef CONFIG_USE_CPU_BARRIER
	smp_mb();
#else
	/* Prevent compiler reordering. */
	barrier();
#endif
	r1 = y;
	up(&sem_end);
}

static void ordering_thread_fn_cpu1(void)
{
	down(&sem_y);
	y = 1;
#ifdef CONFIG_USE_CPU_BARRIER
	smp_mb();
#else
	/* Prevent compiler reordering. */
	barrier();
#endif
	r2 = x;
	up(&sem_end);
}

/* The Watcher */
static void ordering_thread_fn_cpu2(void)
{
	static unsigned int detected;

	/* Reset x and y. */
	x = 0;
	y = 0;

	up(&sem_x);
	up(&sem_y);

	down(&sem_end);
	down(&sem_end);

	if (r1 == 0 && r2 == 0)
		pr_info("%d reorders detected\n", ++detected);
}
#endif
static void ordering_thread_fn(unsigned int cpu)
{
	switch (cpu) {
	case 0:
		ordering_thread_fn_cpu0();
		break;
	case 1:
		ordering_thread_fn_cpu1();
		break;
	case 2:
		ordering_thread_fn_cpu2();
		break;
	default:
		break;
	}
}

static int ordering_should_run(unsigned int cpu)
{
	if (likely(cpu < 3))
		return true;
	return false;
}

static struct smp_hotplug_thread ordering_smp_thread = {
	.store			= &ordering_tasks,
	.thread_should_run	= ordering_should_run,
	.thread_fn		= ordering_thread_fn,
	.thread_comm		= "ordering/%u",
};

static int __init ordering_init(void)
{
#ifndef CONFIG_ORDERING_RMO
	sema_init(&sem_x, 0);
	sema_init(&sem_y, 0);
	sema_init(&sem_end, 0);
#endif

	return smpboot_register_percpu_thread(&ordering_smp_thread);
}

static void ordering_exit(void)
{
	smpboot_unregister_percpu_thread(&ordering_smp_thread);
}

module_init(ordering_init);
module_exit(ordering_exit);

MODULE_AUTHOR(CONFIG_MODULE_AUTHOR);
MODULE_VERSION(CONFIG_MODULE_VERSION);
MODULE_LICENSE(CONFIG_MODULE_LICENSE);
MODULE_DESCRIPTION(CONFIG_MODULE_DESCRIPTION);
