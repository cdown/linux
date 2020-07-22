// SPDX-License-Identifier: GPL-2.0-only
/*
 * UFS Exynos debugging functions
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 * Author: Kiwoong Kim <kwmad.kim@samsung.com>
 *
 */
#include <linux/platform_device.h>
#include <linux/module.h>
#include <asm-generic/unaligned.h>
#include "ufshcd.h"
#include "ufs-exynos-if.h"

#define MAX_CMD_LOGS    32

struct cmd_data {
	unsigned int tag;
	u32 sct;
	u64 lba;
	u64 start_time;
	u64 end_time;
	u64 outstanding_reqs;
	int retries;
	u8 op;
};

struct ufs_cmd_info {
	u32 total;
	u32 last;
	struct cmd_data data[MAX_CMD_LOGS];
	struct cmd_data *pdata[MAX_CMD_LOGS];
};

/*
 * This structure points out several contexts on debugging
 * per one host instant.
 * Now command history exists in here but later handle may
 * contains some mmio base addresses including vendor specific
 * regions to get hardware contexts.
 */
struct ufs_s_dbg_mgr {
	struct ufs_exynos_handle *handle;
	int active;
	u64 first_time;
	u64 time;

	/* cmd log */
	struct ufs_cmd_info cmd_info;
	struct cmd_data cmd_log;		/* temp buffer to put */
	spinlock_t cmd_lock;
};

static void ufs_s_print_cmd_log(struct ufs_s_dbg_mgr *mgr, struct device *dev)
{
	struct ufs_cmd_info *cmd_info = &mgr->cmd_info;
	struct cmd_data *data;
	u32 i, idx;
	u32 last;
	u32 max = MAX_CMD_LOGS;
	unsigned long flags;
	u32 total;

	spin_lock_irqsave(&mgr->cmd_lock, flags);
	total = cmd_info->total;
	if (cmd_info->total < max)
		max = cmd_info->total;
	last = (cmd_info->last + MAX_CMD_LOGS - 1) % MAX_CMD_LOGS;
	spin_unlock_irqrestore(&mgr->cmd_lock, flags);

	dev_err(dev, ":---------------------------------------------------\n");
	dev_err(dev, ":\t\tSCSI CMD(%u)\n", total - 1);
	dev_err(dev, ":---------------------------------------------------\n");
	dev_err(dev, ":OP, TAG, LBA, SCT, RETRIES, STIME, ETIME, REQS\n\n");

	idx = (last == max - 1) ? 0 : last + 1;
	data = &cmd_info->data[idx];
	for (i = 0 ; i < max ; i++, data = &cmd_info->data[idx]) {
		dev_err(dev, ": 0x%02x, %02d, 0x%08llx, 0x%04x, %d, %llu, %llu, 0x%llx",
			data->op, data->tag, data->lba, data->sct, data->retries,
			data->start_time, data->end_time, data->outstanding_reqs);
		idx = (idx == max - 1) ? 0 : idx + 1;
	}
}

static void ufs_s_put_cmd_log(struct ufs_s_dbg_mgr *mgr,
			      struct cmd_data *cmd_data)
{
	struct ufs_cmd_info *cmd_info = &mgr->cmd_info;
	unsigned long flags;
	struct cmd_data *pdata;

	spin_lock_irqsave(&mgr->cmd_lock, flags);
	pdata = &cmd_info->data[cmd_info->last];
	++cmd_info->total;
	++cmd_info->last;
	cmd_info->last = cmd_info->last % MAX_CMD_LOGS;
	spin_unlock_irqrestore(&mgr->cmd_lock, flags);

	pdata->op = cmd_data->op;
	pdata->tag = cmd_data->tag;
	pdata->lba = cmd_data->lba;
	pdata->sct = cmd_data->sct;
	pdata->retries = cmd_data->retries;
	pdata->start_time = cmd_data->start_time;
	pdata->end_time = 0;
	pdata->outstanding_reqs = cmd_data->outstanding_reqs;
	cmd_info->pdata[cmd_data->tag] = pdata;
}

/*
 * EXTERNAL FUNCTIONS
 *
 * There are two classes that are to initialize data structures for debug
 * and to define actual behavior.
 */
void exynos_ufs_dump_info(struct ufs_exynos_handle *handle, struct device *dev)
{
	struct ufs_s_dbg_mgr *mgr = (struct ufs_s_dbg_mgr *)handle->private;

	if (mgr->active == 0)
		goto out;

	mgr->time = cpu_clock(raw_smp_processor_id());

	ufs_s_print_cmd_log(mgr, dev);

	if (mgr->first_time == 0ULL)
		mgr->first_time = mgr->time;
out:
	return;
}

void exynos_ufs_cmd_log_start(struct ufs_exynos_handle *handle,
			      struct ufs_hba *hba, int tag)
{
	struct ufs_s_dbg_mgr *mgr = (struct ufs_s_dbg_mgr *)handle->private;
	struct scsi_cmnd *cmd = hba->lrb[tag].cmd;
	int cpu = raw_smp_processor_id();
	struct cmd_data *cmd_log = &mgr->cmd_log;	/* temp buffer to put */
	u64 lba;
	u32 sct;

	if (mgr->active == 0)
		return;

	cmd_log->start_time = cpu_clock(cpu);
	cmd_log->op = cmd->cmnd[0];
	cmd_log->tag = tag;

	/* This function runtime is protected by spinlock from outside */
	cmd_log->outstanding_reqs = hba->outstanding_reqs;

	/* Now assume using WRITE_10 and READ_10 */
	lba = get_unaligned_be32(&cmd->cmnd[2]);
	sct = get_unaligned_be32(&cmd->cmnd[7]);
	if (cmd->cmnd[0] != UNMAP)
		cmd_log->lba = lba;

	cmd_log->sct = sct;
	cmd_log->retries = cmd->allowed;

	ufs_s_put_cmd_log(mgr, cmd_log);
}

void exynos_ufs_cmd_log_end(struct ufs_exynos_handle *handle,
			    struct ufs_hba *hba, int tag)
{
	struct ufs_s_dbg_mgr *mgr = (struct ufs_s_dbg_mgr *)handle->private;
	struct ufs_cmd_info *cmd_info = &mgr->cmd_info;
	int cpu = raw_smp_processor_id();

	if (mgr->active == 0)
		return;

	cmd_info->pdata[tag]->end_time = cpu_clock(cpu);
}

int exynos_ufs_init_dbg(struct ufs_exynos_handle *handle, struct device *dev)
{
	struct ufs_s_dbg_mgr *mgr;

	mgr = devm_kzalloc(dev, sizeof(struct ufs_s_dbg_mgr), GFP_KERNEL);
	if (!mgr)
		return -ENOMEM;
	handle->private = (void *)mgr;
	mgr->handle = handle;
	mgr->active = 1;

	/* cmd log */
	spin_lock_init(&mgr->cmd_lock);

	return 0;
}
MODULE_AUTHOR("Kiwoong Kim <kwmad.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos UFS debug information");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1");
