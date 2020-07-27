// SPDX-License-Identifier: GPL-2.0
/*
 * printk/index.c - Userspace indexing of printk formats
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>

/**
 * struct pi_sec - printk index section metadata
 *
 * @file:  The debugfs file where userspace can index these printk formats
 * @start: Section start boundary
 * @end:   Section end boundary
 *
 * Allocated and populated by pi_sec_store.
 *
 * @mod is NULL if the printk formats in question are built in to vmlinux
 * itself.
 *
 * @file may be an ERR_PTR value if the file or one of its ancestors was not
 * successfully created.
 */
struct pi_sec {
	struct dentry *file;
	struct pi_object *start;
	struct pi_object *end;
};

/* The base dir for module formats, typically debugfs/printk/index/ */
struct dentry *dfs_index;

#ifdef CONFIG_MODULES
static const char *pi_get_module_name(struct module *mod)
{
	return mod ? mod->name : "vmlinux";
}

void pi_sec_remove(struct module *mod)
{
	if (!mod || !mod->pi_sec)
		return;

	debugfs_remove(mod->pi_sec->file);
	kfree(mod->pi_sec);
	mod->pi_sec = NULL;
}
#else
static const char *pi_get_module_name(struct module *mod)
{
	return "vmlinux";
}

void pi_sec_remove(struct module *mod)
{
}
#endif

static void *pi_next(struct seq_file *s, void *v, loff_t *pos)
{
	const struct pi_sec *ps = s->file->f_inode->i_private;
	struct pi_object *pi = NULL;
	loff_t idx = *pos - 1;

	++*pos;

	if (idx == -1)
		return SEQ_START_TOKEN;

	pi = ps->start + idx;

	return pi < ps->end ? pi : NULL;
}

static void *pi_start(struct seq_file *s, loff_t *pos)
{
	return pi_next(s, NULL, pos);
}

static int pi_show(struct seq_file *s, void *v)
{
	const struct pi_object *pi = v;
	int level = LOGLEVEL_DEFAULT;
	enum log_flags lflags = 0;
	u16 prefix_len;

	if (v == SEQ_START_TOKEN) {
		seq_puts(s,
			 "# <level[,flags]> filename:line function \"format\"\n");
		return 0;
	}

	prefix_len = parse_prefix(pi->fmt, &level, &lflags);
	seq_printf(s, "<%d%s> %s:%d %s \"",
			level, lflags & LOG_CONT ? ",c" : "", pi->file,
			pi->line, pi->func);
	seq_escape_printf_format(s, pi->fmt + prefix_len);
	seq_puts(s, "\"\n");

	return 0;
}

static void pi_stop(struct seq_file *p, void *v)
{
}

static const struct seq_operations dfs_index_seq_ops = {
	.start = pi_start,
	.next  = pi_next,
	.show  = pi_show,
	.stop  = pi_stop,
};


static int pi_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &dfs_index_seq_ops);
}

static const struct file_operations dfs_index_fops = {
	.open    = pi_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release
};


void pi_sec_store(struct module *mod)
{
	struct pi_sec *ps = NULL;
	struct pi_object *start = NULL, *end = NULL;

	ps = kmalloc(sizeof(struct pi_sec), GFP_KERNEL);
	if (!ps)
		return;

#ifdef CONFIG_MODULES
	if (mod) {
		start = mod->printk_index_start;
		end = start + mod->printk_index_size;
	}
#endif

	if (!mod) {
		/* vmlinux */
		start = __start_printk_index;
		end = __stop_printk_index;
	}

	ps->start = start;
	ps->end = end;
	ps->file = debugfs_create_file(pi_get_module_name(mod), 0444, dfs_index,
				       ps, &dfs_index_fops);

	if (IS_ERR(ps->file)) {
		pi_sec_remove(mod);
		return;
	}

#ifdef CONFIG_MODULES
	if (mod)
		mod->pi_sec = ps;
#endif

	/*
	 * vmlinux's pi_sec is only accessible as private data on the inode,
	 * since we never have to free it.
	 */
}

static int __init pi_init(void)
{
	struct dentry *dfs_root = debugfs_create_dir("printk", NULL);

	dfs_index = debugfs_create_dir("index", dfs_root);
	pi_sec_store(NULL);

	return 0;
}

core_initcall(pi_init);
