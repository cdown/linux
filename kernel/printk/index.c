// SPDX-License-Identifier: GPL-2.0
/*
 * printk/index.c - Userspace indexing of printk formats
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>

/* The base dir for module formats, typically debugfs/printk/index/ */
static struct dentry *dfs_index;

#ifdef CONFIG_MODULES
static const char *pi_get_module_name(struct module *mod)
{
	return mod ? mod->name : "vmlinux";
}
#else
static const char *pi_get_module_name(struct module *mod)
{
	return "vmlinux";
}
#endif

static struct pi_object *pi_get_entry(const struct module *mod, loff_t pos)
{
	struct pi_object *entries;
	unsigned int nr_entries;

	if (mod) {
		entries = mod->printk_index_start;
		nr_entries = mod->printk_index_size;
	} else {
		/* vmlinux, comes from linker symbols */
		entries = __start_printk_index;
		nr_entries = __stop_printk_index - __start_printk_index;
	}

	if (pos >= nr_entries)
		return NULL;

	return &entries[pos];
}

static void *pi_next(struct seq_file *s, void *v, loff_t *pos)
{
	const struct module *mod = s->file->f_inode->i_private;
	struct pi_object *entry = pi_get_entry(mod, *pos);

	(*pos)++;

	return entry;
}

static void *pi_start(struct seq_file *s, loff_t *pos)
{
	/*
	 * Make show() print the header line. Do not update *pos because
	 * pi_next() still has to return the entry at index 0 later.
	 */
	if (*pos == 0)
		return SEQ_START_TOKEN;

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

	if (!pi->fmt) {
		/*
		 * TODO: This happens on GCC 10.2.0 only for "BUG: kernel NULL
		 * pointer dereference, address: %px\n", but only when the
		 * struct is const + __used. What's up with that? How does it
		 * even get past the first __builtin_constant_p guard?
		 */
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


void pi_create_file(struct module *mod)
{
	debugfs_create_file(pi_get_module_name(mod), 0444, dfs_index,
				       mod, &dfs_index_fops);
}

void pi_remove_file(struct module *mod)
{
	debugfs_remove(debugfs_lookup(pi_get_module_name(mod), dfs_index));
}

static int __init pi_init(void)
{
	struct dentry *dfs_root = debugfs_create_dir("printk", NULL);

	dfs_index = debugfs_create_dir("index", dfs_root);
	pi_create_file(NULL);

	return 0;
}

core_initcall(pi_init);
