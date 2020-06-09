// SPDX-License-Identifier: GPL-2.0
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/cpumask.h>
#include <linux/export.h>
#include <linux/memblock.h>
#include <linux/numa.h>
#include <linux/spinlock.h>

/**
 * cpumask_next - get the next cpu in a cpumask
 * @n: the cpu prior to the place to search (ie. return will be > @n)
 * @srcp: the cpumask pointer
 *
 * Returns >= nr_cpu_ids if no further cpus set.
 */
unsigned int cpumask_next(int n, const struct cpumask *srcp)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpumask_check(n);
	return find_next_bit(cpumask_bits(srcp), nr_cpumask_bits, n + 1);
}
EXPORT_SYMBOL(cpumask_next);

/**
 * cpumask_next_and - get the next cpu in *src1p & *src2p
 * @n: the cpu prior to the place to search (ie. return will be > @n)
 * @src1p: the first cpumask pointer
 * @src2p: the second cpumask pointer
 *
 * Returns >= nr_cpu_ids if no further cpus set in both.
 */
int cpumask_next_and(int n, const struct cpumask *src1p,
		     const struct cpumask *src2p)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpumask_check(n);
	return find_next_and_bit(cpumask_bits(src1p), cpumask_bits(src2p),
		nr_cpumask_bits, n + 1);
}
EXPORT_SYMBOL(cpumask_next_and);

/**
 * cpumask_any_but - return a "random" in a cpumask, but not this one.
 * @mask: the cpumask to search
 * @cpu: the cpu to ignore.
 *
 * Often used to find any cpu but smp_processor_id() in a mask.
 * Returns >= nr_cpu_ids if no cpus set.
 */
int cpumask_any_but(const struct cpumask *mask, unsigned int cpu)
{
	unsigned int i;

	cpumask_check(cpu);
	for_each_cpu(i, mask)
		if (i != cpu)
			break;
	return i;
}
EXPORT_SYMBOL(cpumask_any_but);

/**
 * cpumask_next_wrap - helper to implement for_each_cpu_wrap
 * @n: the cpu prior to the place to search
 * @mask: the cpumask pointer
 * @start: the start point of the iteration
 * @wrap: assume @n crossing @start terminates the iteration
 *
 * Returns >= nr_cpu_ids on completion
 *
 * Note: the @wrap argument is required for the start condition when
 * we cannot assume @start is set in @mask.
 */
int cpumask_next_wrap(int n, const struct cpumask *mask, int start, bool wrap)
{
	int next;

again:
	next = cpumask_next(n, mask);

	if (wrap && n < start && next >= start) {
		return nr_cpumask_bits;

	} else if (next >= nr_cpumask_bits) {
		wrap = true;
		n = -1;
		goto again;
	}

	return next;
}
EXPORT_SYMBOL(cpumask_next_wrap);

/* These are not inline because of header tangles. */
#ifdef CONFIG_CPUMASK_OFFSTACK
/**
 * alloc_cpumask_var_node - allocate a struct cpumask on a given node
 * @mask: pointer to cpumask_var_t where the cpumask is returned
 * @flags: GFP_ flags
 *
 * Only defined when CONFIG_CPUMASK_OFFSTACK=y, otherwise is
 * a nop returning a constant 1 (in <linux/cpumask.h>)
 * Returns TRUE if memory allocation succeeded, FALSE otherwise.
 *
 * In addition, mask will be NULL if this fails.  Note that gcc is
 * usually smart enough to know that mask can never be NULL if
 * CONFIG_CPUMASK_OFFSTACK=n, so does code elimination in that case
 * too.
 */
bool alloc_cpumask_var_node(cpumask_var_t *mask, gfp_t flags, int node)
{
	*mask = kmalloc_node(cpumask_size(), flags, node);

#ifdef CONFIG_DEBUG_PER_CPU_MAPS
	if (!*mask) {
		printk(KERN_ERR "=> alloc_cpumask_var: failed!\n");
		dump_stack();
	}
#endif

	return *mask != NULL;
}
EXPORT_SYMBOL(alloc_cpumask_var_node);

bool zalloc_cpumask_var_node(cpumask_var_t *mask, gfp_t flags, int node)
{
	return alloc_cpumask_var_node(mask, flags | __GFP_ZERO, node);
}
EXPORT_SYMBOL(zalloc_cpumask_var_node);

/**
 * alloc_cpumask_var - allocate a struct cpumask
 * @mask: pointer to cpumask_var_t where the cpumask is returned
 * @flags: GFP_ flags
 *
 * Only defined when CONFIG_CPUMASK_OFFSTACK=y, otherwise is
 * a nop returning a constant 1 (in <linux/cpumask.h>).
 *
 * See alloc_cpumask_var_node.
 */
bool alloc_cpumask_var(cpumask_var_t *mask, gfp_t flags)
{
	return alloc_cpumask_var_node(mask, flags, NUMA_NO_NODE);
}
EXPORT_SYMBOL(alloc_cpumask_var);

bool zalloc_cpumask_var(cpumask_var_t *mask, gfp_t flags)
{
	return alloc_cpumask_var(mask, flags | __GFP_ZERO);
}
EXPORT_SYMBOL(zalloc_cpumask_var);

/**
 * alloc_bootmem_cpumask_var - allocate a struct cpumask from the bootmem arena.
 * @mask: pointer to cpumask_var_t where the cpumask is returned
 *
 * Only defined when CONFIG_CPUMASK_OFFSTACK=y, otherwise is
 * a nop (in <linux/cpumask.h>).
 * Either returns an allocated (zero-filled) cpumask, or causes the
 * system to panic.
 */
void __init alloc_bootmem_cpumask_var(cpumask_var_t *mask)
{
	*mask = memblock_alloc(cpumask_size(), SMP_CACHE_BYTES);
	if (!*mask)
		panic("%s: Failed to allocate %u bytes\n", __func__,
		      cpumask_size());
}

/**
 * free_cpumask_var - frees memory allocated for a struct cpumask.
 * @mask: cpumask to free
 *
 * This is safe on a NULL mask.
 */
void free_cpumask_var(cpumask_var_t mask)
{
	kfree(mask);
}
EXPORT_SYMBOL(free_cpumask_var);

/**
 * free_bootmem_cpumask_var - frees result of alloc_bootmem_cpumask_var
 * @mask: cpumask to free
 */
void __init free_bootmem_cpumask_var(cpumask_var_t mask)
{
	memblock_free_early(__pa(mask), cpumask_size());
}
#endif

static void calc_node_distance(int *node_dist, int node)
{
	int i;

	for (i = 0; i < nr_node_ids; i++)
		node_dist[i] = node_distance(node, i);
}

static int find_nearest_node(int *node_dist, bool *used)
{
	int i, min_dist = node_dist[0], node_id = -1;

	/* Choose the first unused node to compare */
	for (i = 0; i < nr_node_ids; i++) {
		if (used[i] == 0) {
			min_dist = node_dist[i];
			node_id = i;
			break;
		}
	}

	/* Compare and return the nearest node */
	for (i = 0; i < nr_node_ids; i++) {
		if (node_dist[i] < min_dist && used[i] == 0) {
			min_dist = node_dist[i];
			node_id = i;
		}
	}

	return node_id;
}

static unsigned int __cpumask_local_spread(unsigned int i, int node)
{
	int cpu;

	/* Wrap: we always want a cpu. */
	i %= num_online_cpus();

	if (node == NUMA_NO_NODE) {
		for_each_cpu(cpu, cpu_online_mask)
			if (i-- == 0)
				return cpu;
	} else {
		/* NUMA first. */
		for_each_cpu_and(cpu, cpumask_of_node(node), cpu_online_mask)
			if (i-- == 0)
				return cpu;

		for_each_cpu(cpu, cpu_online_mask) {
			/* Skip NUMA nodes, done above. */
			if (cpumask_test_cpu(cpu, cpumask_of_node(node)))
				continue;

			if (i-- == 0)
				return cpu;
		}
	}
	BUG();
}

/**
 * cpumask_local_spread - select the i'th cpu with local numa cpu's first
 * @i: index number
 * @node: local numa_node
 *
 * This function selects an online CPU according to a numa aware policy;
 * local cpus are returned first, followed by the nearest non-local ones,
 * then it wraps around.
 *
 * It's not very efficient, but useful for setup.
 */
unsigned int cpumask_local_spread(unsigned int i, int node)
{
	static DEFINE_SPINLOCK(spread_lock);
	static int node_dist[MAX_NUMNODES];
	static bool used[MAX_NUMNODES];
	unsigned long flags;
	int cpu, j, id;

	/* Wrap: we always want a cpu. */
	i %= num_online_cpus();

	if (node == NUMA_NO_NODE) {
		for_each_cpu(cpu, cpu_online_mask)
			if (i-- == 0)
				return cpu;
	} else {
		if (nr_node_ids > MAX_NUMNODES)
			return __cpumask_local_spread(i, node);

		spin_lock_irqsave(&spread_lock, flags);
		memset(used, 0, nr_node_ids * sizeof(bool));
		calc_node_distance(node_dist, node);
		for (j = 0; j < nr_node_ids; j++) {
			id = find_nearest_node(node_dist, used);
			if (id < 0)
				break;

			for_each_cpu_and(cpu, cpumask_of_node(id),
					 cpu_online_mask)
				if (i-- == 0) {
					spin_unlock_irqrestore(&spread_lock,
							       flags);
					return cpu;
				}
			used[id] = 1;
		}
		spin_unlock_irqrestore(&spread_lock, flags);

		for_each_cpu(cpu, cpu_online_mask)
			if (i-- == 0)
				return cpu;
	}
	BUG();
}
EXPORT_SYMBOL(cpumask_local_spread);

static DEFINE_PER_CPU(int, distribute_cpu_mask_prev);

/**
 * Returns an arbitrary cpu within srcp1 & srcp2.
 *
 * Iterated calls using the same srcp1 and srcp2 will be distributed within
 * their intersection.
 *
 * Returns >= nr_cpu_ids if the intersection is empty.
 */
int cpumask_any_and_distribute(const struct cpumask *src1p,
			       const struct cpumask *src2p)
{
	int next, prev;

	/* NOTE: our first selection will skip 0. */
	prev = __this_cpu_read(distribute_cpu_mask_prev);

	next = cpumask_next_and(prev, src1p, src2p);
	if (next >= nr_cpu_ids)
		next = cpumask_first_and(src1p, src2p);

	if (next < nr_cpu_ids)
		__this_cpu_write(distribute_cpu_mask_prev, next);

	return next;
}
EXPORT_SYMBOL(cpumask_any_and_distribute);
