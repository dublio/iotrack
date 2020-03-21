/**
 * iotrack - disk io tracking tool
 *
 * It extend iostat to more fileds and track statistics for
 * blk-iotrack.
 *
 * Author: Weiping Zhang <zwp10758@gmail.com>
 * Date: 2020-03-12
 */

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <getopt.h>
#include "list.h"
#include "iotrack.h"

LIST_HEAD(g_block_device);
LIST_HEAD(g_block_cgroup);

int g_index;
int g_loop;
int g_extend;
int g_debug;
char g_ts[64];
const char *g_name;

unsigned long long g_delta_time_ms = 1000;

struct block_cgroup *g_block_cgroup_root;
char g_block_cgroup_root_path[PATH_MAX];


static void block_device_deinit_one(struct block_device *dev);
static inline struct block_gq *block_gq_lookup(struct block_cgroup *g,
				int major, int minor);
static void block_gq_deinit_one(struct block_gq *gq);

#define log(fmt, ...) \
do { \
	fprintf(stderr, "%s[%d] "fmt, __func__, __LINE__, ## __VA_ARGS__); \
} while(0)

#define dbg(fmt, ...) \
do { \
	if (g_debug) \
		fprintf(stderr, "%s[%d] "fmt, __func__, __LINE__, ## __VA_ARGS__); \
} while(0)

static void block_cgroup_root_path_init(const char *path)
{
	snprintf(g_block_cgroup_root_path, sizeof(g_block_cgroup_root_path),
		"%s", path);
}

static void __attribute__((constructor)) iotrack_pre_setup(void)
{
	char file[PATH_MAX];
	struct stat s;
	int ret;

	/* check /sys/fs/cgroup/blkio/blkio.iotrack.stat */
	snprintf(file, sizeof(file), "%s/%s", BLOCK_CGROUP_ROOT_V1,
			IOTRACK_STAT_FILE_V1);

	dbg("detect: %s\n", file);
	ret = stat(file, &s);
	if (!ret) {
		block_cgroup_root_path_init(BLOCK_CGROUP_ROOT_V1);
		return;
	}

	/*
	 * check /sys/fs/cgroup/io.iotrack.stat, cgroup v2 mount point
	 * may not be this for some OS, user should use -r to speicy
	 * that.
	 */
	snprintf(file, sizeof(file), "%s/%s", BLOCK_CGROUP_ROOT_V2,
			IOTRACK_STAT_FILE_V2);

	dbg("detect: %s\n", file);
	ret = stat(file, &s);
	if (!ret) {
		block_cgroup_root_path_init(BLOCK_CGROUP_ROOT_V2);
		return;
	}
}

static inline mode_t stat_mode(const char *path)
{
	struct stat st;

	if (stat(path, &st))
		return (mode_t)0;

	return st.st_mode;
}

static inline void *zmalloc(size_t size)
{
	void *buf;

	buf = malloc(size);
	if (!buf)
		return NULL;

	memset(buf, 0, size);

	return buf;
}

static int iostat_init(struct block_device *dev)
{
	int fd;
	char file[PATH_MAX];

	snprintf(file, sizeof(file), "%s/%s/stat", BLOCK_DEVICE_DIR, dev->name);
	fd = open(file, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "failed to open %s\n", file);
		return -1;
	}

	dev->fd_iostat = fd;

	return 0;
}

static void iostat_deinit(struct block_device *dev)
{
	close(dev->fd_iostat);
}

static int iostat_read(struct block_device *dev)
{
	char buf[LINE_MAX];
	int len;
	struct iostat s;

	memset(buf, 0, sizeof(buf));

	len = pread(dev->fd_iostat, buf, sizeof(buf), 0);
	if (len <= 0) {
		fprintf(stderr, "failed to read iostat for %s\n", dev->name);
		return -1;
	}

	if (IOSTAT_FIELD_NR != sscanf(buf,
			"%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
			&s.r_ios, &s.r_merge, &s.r_sector, &s.r_ticks,
			&s.w_ios, &s.w_merge, &s.w_sector, &s.w_ticks,
			&s.in_flight, &s.io_ticks, &s.time_in_queue)) {
		fprintf(stderr, "failed to parse iostat %s\n", buf);
		return -1;
	}

	dev->iostat[g_index] = s;

	return 0;
}

static void iostat_calc(struct block_device *dev)
{
	struct iostat *now = &dev->iostat[g_index];
	struct iostat *last = &dev->iostat[1 - g_index];
	float delta_io_ticks = now->io_ticks - last->io_ticks;
	float delta_time_in_queue = now->time_in_queue - last->time_in_queue;
	float t_r_ios, t_w_ios, t_r_sector, t_w_sector, t_r_ticks, t_w_ticks;

	dev->rrqm = (float)(1000 * (now->r_merge - last->r_merge)) /
				(float)g_delta_time_ms;
	dev->wrqm = (float)(1000 * (now->w_merge - last->w_merge)) /
				(float)g_delta_time_ms;
	t_r_ios = (now->r_ios - last->r_ios);
	dev->r = (1000.0 * t_r_ios) / (float)g_delta_time_ms;

	t_w_ios = (now->w_ios - last->w_ios);
	dev->w = (1000.0 * t_w_ios) / (float)g_delta_time_ms;

	t_r_sector = (now->r_sector - last->r_sector);
	dev->rmb = (float)(1000 * t_r_sector) / (float)(2 * 1024) /
				(float)g_delta_time_ms;

	t_w_sector = (now->w_sector - last->w_sector);
	dev->wmb = (float)(1000 * t_w_sector) / (float)(2 * 1024) /
				(float)g_delta_time_ms;

	dev->avgrqkb = (dev->r + dev->w) > 0 ?
		(t_r_sector + t_w_sector) / 2.0 / (dev->r + dev->w) : 0.0;
	dev->avgqu = (float) (now->time_in_queue - last->time_in_queue) /
				(float)g_delta_time_ms;
	t_r_ticks = now->r_ticks - last->r_ticks;
	t_w_ticks = now->w_ticks - last->w_ticks;

	dev->await = (t_r_ios + t_w_ios) ? 
			(t_r_ticks + t_w_ticks) / (t_r_ios + t_w_ios) : 0.0; 
	dev->r_await = t_r_ios ? t_r_ticks / t_r_ios : 0.0;
	dev->w_await = t_w_ios ? t_w_ticks / t_w_ios : 0.0;
	dev->util = 100 * delta_io_ticks / (float)g_delta_time_ms;
	dev->conc = delta_io_ticks > 0 ?
			delta_time_in_queue / delta_io_ticks : 0.0;
}

static inline void iostat_show_header(void)
{
	if (g_loop && g_loop % HEADER_PER_DATA != 0)
		return;

	fprintf(stderr, "%-24s %-16s "		/* timestamp, name */
			"%8s %8s "	/* rrqm/s   wrqm/s */
			"%8s %8s "	/* r/s     w/s */
			"%8s %8s "	/* rMB/s    wMB/s */
			"%8s %8s "	/* avgrqkb avgqu-sz */
			"%8s %8s %8s "	/* await r_await w_await */
			"%8s %8s "	/* svctm  %util */
			"%8s\n"		/* conc */
			,
			"Time", "Device",
			"rrqm/s", "wrqm/s",
			"r/s", "w/s",
			"rMB/s", "wMB/s",
			"avgrqkb", "avgqu-sz",
			"await", "r_await", "w_await",
			"svctm", "%util",
			"conc"
			);
}

static void iostat_show(struct block_device *dev)
{
	fprintf(stderr, "%-24s %-16s "		/* timestamp, name */
			"%8.2f %8.2f "	/* rrqm/s   wrqm/s */
			"%8.2f %8.2f "	/* r/s     w/s */
			"%8.2f %8.2f "	/* rMB/s    wMB/s */
			"%8.2f %8.2f "	/* avgrqkb avgqu-sz */
			"%8.2f %8.2f %8.2f "	/* await r_await w_await */
			"%8.2f %8.2f "	/* svctm  %util */
			"%8.2f\n"		/* conc */
			,
			g_ts, dev->name,		/* timestamp, name */
			dev->rrqm, dev->wrqm,		/* rrqm/s   wrqm/s */
			dev->r, dev->w,			/* r/s     w/s */
			dev->rmb, dev->wmb,		/* rMB/s    wMB/s */
			dev->avgrqkb, dev->avgqu,	/* avgrqkb avgqu-sz */
			dev->await, dev->r_await, dev->w_await,
			0.0, dev->util,			/* svctm  %util */
			dev->conc			/* conc */
			); 
}

static int block_device_read_iostat(void)
{
	struct block_device *dev, *tmp;

	/* read block device iostat */
	list_for_each_entry_safe(dev, tmp, &g_block_device, node) {
		if (iostat_read(dev))
			block_device_deinit_one(dev);
	}

	return 0;
}

static void block_device_calc_iostat(void)
{
	struct block_device *dev;

	/* calculate block device iostat */
	list_for_each_entry(dev, &g_block_device, node)
		iostat_calc(dev);
}

static void block_device_show_iostat(void)
{
	struct block_device *dev;

	iostat_show_header();

	/* calculate block device iostat */
	list_for_each_entry(dev, &g_block_device, node)
		iostat_show(dev);
}

static void block_device_deinit_one(struct block_device *dev)
{
	struct block_cgroup *g;
	struct block_gq *gq;

	dbg("del block device %s %d:%d\n", dev->name,
		dev->major, dev->minor);

	/* remove all block_gq related to this block_device */
	list_for_each_entry(g, &g_block_cgroup, node) {
		gq = block_gq_lookup(g, dev->major, dev->minor);
		if (gq)
			block_gq_deinit_one(gq);
	}

	iostat_deinit(dev);
	list_del(&dev->node);
	free(dev);
}

static void block_device_deinit(void)
{
	struct block_device *dev, *tmp;

	list_for_each_entry_safe(dev, tmp, &g_block_device, node)
		block_device_deinit_one(dev);
}

static int block_device_init_one(const char *name)
{
	struct block_device *dev;
	int fd, len, major, minor;
	char tmp[64], file[PATH_MAX];

	dev = zmalloc(sizeof(*dev));
	if (!dev) {
		fprintf(stderr, "failed to alloc memory\n");
		return -1;
	}

	strncpy(dev->name, name, sizeof(dev->name) - 1);

	memset(tmp, 0, sizeof(tmp));

	/* get major and minor */
	snprintf(file, sizeof(file), "%s/%s/dev", BLOCK_DEVICE_DIR, name);

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "failed to open %s\n", file);
		goto free;
	}

	len = read(fd, tmp, sizeof(tmp));
	if (len <= 0) {
		fprintf(stderr, "failed to read %s\n", file);
		goto close;
	}

	if (2 != sscanf(tmp, "%d:%d", &major, &minor)) {
		fprintf(stderr, "failed to pase major:minor %s\n", tmp);
		goto close;
	}

	dev->major = major;
	dev->minor = minor;

	dbg("add block device %s %d:%d\n", dev->name,
		dev->major, dev->minor);

	/* init iostat */
	if (iostat_init(dev))
		goto close;

	list_add_tail(&dev->node, &g_block_device);

	close(fd);
	return 0;

close:
	close(fd);
free:
	free(dev);
	return -1;
}

static int block_device_init_all(void)
{
	DIR *dirp;
	struct dirent *entry;
	char tmp[PATH_MAX];
	mode_t mode;
	const char *dir_path = BLOCK_DEVICE_DIR;

	dirp = opendir(dir_path);
	if (!dirp) {
		fprintf(stderr, "failed to open %s\n", dir_path);
		return -1;
	}

	for (;;) {
		errno = 0;
		entry = readdir(dirp);
		if (!entry && errno) {
			fprintf(stderr, "failed to readdir %s\n", dir_path);
			goto out;
		}

		/* end of directory stream is reached */
		if (NULL == entry)
			break;

		/* skip . and .. */
		if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name))
			continue;

		snprintf(tmp, sizeof(tmp), "%s/%s", dir_path, entry->d_name);
		mode = stat_mode(tmp);

		/* if it's a directory iterate it recursively */
		if (S_ISDIR(mode)) {
			if (block_device_init_one(entry->d_name)) {
				goto out;
			}
		}
	}

	closedir(dirp);
	return 0;

out:
	block_device_deinit();
	closedir(dirp);

	return -1;
}

static int block_device_init(void)
{

	/*
	 * if list is not empty, user must specify the device by -d,
	 * just return here, don't scan all block device.
	 */
	if (0 == list_empty(&g_block_device))
		return 0;

	/* scan all block device in the system */
	return block_device_init_all();
}

static inline bool block_cgroup_is_exist(const char *path)
{
	struct block_cgroup *g;

	list_for_each_entry(g, &g_block_cgroup, node) {
		if (!strcmp(g->path, path))
			return true;
	}

	return false;
}
/**
 * monitor blk-iotrak statistics
 *
 * @path: /sys/fs/cgroup/blkio/xxx/
 */
static int block_cgroup_alloc_one(const char *path, bool is_root)
{
	struct block_cgroup *g;

	if (block_cgroup_is_exist(path))
		return 0;

	g = zmalloc(sizeof(*g));
	if (!g) {
		fprintf(stderr, "faied to alloc memory\n");
		return -1;
	}

	/* block_cgroup first stage initialization */
	INIT_LIST_HEAD(&g->node);
	snprintf(g->path, sizeof(g->path), "%s", path);

	dbg("add cgroup %s\n", path);

	/*
	 * _MUST_ use list_add_tail to keep root block_cgroup was the first
	 * element when iterate list by list_for_each_entry.
	 * So the root block_cgroup's data was firstly be read and calculated.
	 */
	if (is_root) {
		g_block_cgroup_root = g;
		list_add(&g->node, &g_block_cgroup);
	} else {
		list_add_tail(&g->node, &g_block_cgroup);
	}

	return 0;
}

static void block_cgroup_free_one(struct block_cgroup *g)
{
	dbg("block cgroup free %s\n", g->path);
	list_del(&g->node);
	free(g);
}

static void block_cgroup_free(void)
{
	struct block_cgroup *g, *tmp;

	list_for_each_entry_safe(g, tmp, &g_block_cgroup, node) {
		block_cgroup_free_one(g);
	}
}


static void block_gq_deinit_one(struct block_gq *gq)
{
	dbg("del block_gq dev:%s, cgroup:%s\n", gq->dev->name, gq->grp->path);
	list_del(&gq->node);
	free(gq);
}

static void block_gq_deinit(struct block_cgroup *g)
{
	struct block_gq *gq, *tmp;

	list_for_each_entry_safe(gq, tmp, &g->gq_head, node) {
		block_gq_deinit_one(gq);
	}
}

static int block_gq_init_one(struct block_cgroup *g, struct block_device *dev)
{
	struct block_gq *gq;

	gq = zmalloc(sizeof(*gq));
	if (!gq) {
		log("alloc memory failed\n");
		goto out;
	}

	gq->dev = dev;
	gq->grp = g;
	INIT_LIST_HEAD(&gq->node);
	list_add_tail(&gq->node, &g->gq_head);

	dbg("add block_gq dev:%s, cgroup:%s\n", dev->name, g->path);

	return 0;
out:
	return -1;
}

static int block_gq_init(struct block_cgroup *g)
{
	struct block_device *dev;

	/* alloc struct block_gq for each block device */
	list_for_each_entry(dev, &g_block_device, node) {
		if (block_gq_init_one(g, dev))
			goto out;
	}

	return 0;
out:
	block_gq_deinit(g);
	return -1;
}

static inline struct block_gq *block_gq_lookup(struct block_cgroup *g,
				int major, int minor)
{
	struct block_gq *gq;

	list_for_each_entry(gq, &g->gq_head, node) {
		if (gq->dev->major == major && gq->dev->minor == minor)
			return gq;
	}

	return NULL;
}

static void block_gq_calc_data(struct block_gq *gq)
{
	struct blk_iotrack *iotrack = &gq->iotrack;
	struct iotrack_stat *now = &iotrack->stat[g_index];
	struct iotrack_stat *last = &iotrack->stat[1 - g_index];
	struct block_device *dev = gq->dev;
	struct block_gq *root_gq;
	int i, j;
	float total, delta, total_device, total_cgroup;
	float total_hit[LAT_BUCKET_NR];

	/* get the root block_gq, it contains the device level statistics */
	root_gq = block_gq_lookup(g_block_cgroup_root, dev->major, dev->minor);
	/* if it is NULL, bug */
	if (!root_gq) {
		log("BUG: not found root block_gq %d:%d\n",
			dev->major, dev->minor);
		_exit(EXIT_FAILURE);
	}

	/* r/s, w/s, o/s */
	total = 0;
	for (i = 0; i < IOT_NR; i++) {
		delta = (float)(now->ios[i] - last->ios[i]);
		iotrack->iops[i] = 1000.0 * delta / (float)g_delta_time_ms;
		iotrack->delta_ios[i] = delta;
		total += delta;
	}
	/* io/s */
	iotrack->iops[IOT_NR] = 1000.0 * total / (float)g_delta_time_ms;
	iotrack->delta_ios[IOT_NR] = total;

	/* rMB/s, wMB/s, oMB/s */
	total = 0;
	for (i = 0; i < IOT_NR; i++) {
		delta = (float)(now->sts[i] - last->sts[i]);
		iotrack->bps[i] = 1000.0 * delta / 2048.0 / (float)g_delta_time_ms;
		iotrack->delta_sts[i] = delta;
		total += delta;
	}
	/* MB/s */
	iotrack->bps[IOT_NR] = 1000.0 * total / 2048.0 / (float)g_delta_time_ms;
	iotrack->delta_sts[IOT_NR] = total;

	/* io percentile: %rio %wio %oio %io */
	total_device = (float)root_gq->iotrack.delta_ios[IOT_NR];
	for (i = 0; i < IOT_NR + 1; i++)
		iotrack->io_pct[i] = total_device > 0 ?
			100.0 * iotrack->delta_ios[i] / total_device : 0.0;

	/* kb percentile: %rkb %wkb %okb %kb */
	total_device = (float)root_gq->iotrack.delta_sts[IOT_NR];
	for (i = 0; i < IOT_NR + 1; i++)
		iotrack->b_pct[i] = total_device > 0 ?
			100.0 * iotrack->delta_sts[i] / total_device : 0.0;

	/* time delta:rtm wtm otm tm */
	total = 0;
	for (i = 0; i < IOT_NR; i++) {
		delta = (float)(now->tms[i] - last->tms[i]);
		iotrack->delta_tms[i] = delta;
		total += delta;
	}
	iotrack->delta_tms[IOT_NR] = total;

	/* time percentile: %rtm %wtm %otm %tm */
	total_device = (float)root_gq->iotrack.delta_tms[IOT_NR];
	for (i = 0; i < IOT_NR + 1; i++) {
		iotrack->tm_pct[i] = total_device > 0 ?
			100.0 * iotrack->delta_tms[i] / total_device : 0.0;
	}

	/* disk time delta:rtm wtm otm tm */
	total = 0;
	for (i = 0; i < IOT_NR; i++) {
		delta = (float)(now->dtms[i] - last->dtms[i]);
		iotrack->delta_dtms[i] = delta;
		total += delta;
	}
	iotrack->delta_dtms[IOT_NR] = total;

	/* disk time percentile: %rtm %wtm %otm %tm */
	total_device = (float)root_gq->iotrack.delta_dtms[IOT_NR];
	for (i = 0; i < IOT_NR + 1; i++) {
		iotrack->dtm_pct[i] = total_device > 0 ?
			100.0 * iotrack->delta_dtms[i] / total_device : 0.0;
	}

	/* %d2c = d2c/q2c = delta_dtms[i]/delta_tms[i] */
	for (i = 0; i < IOT_NR + 1; i++) {
		iotrack->d2c_pct[i] = iotrack->delta_tms[i] > 0 ?
			100.0 * iotrack->delta_dtms[i] / iotrack->delta_tms[i] : 0.0;
	}

	/* cgroup level */
	for (j = 0; j < LAT_BUCKET_NR; j++)
		total_hit[j] = 0.0;

	/*
	 * %rhit0  %rhit1  %rhit2  %rhit3  %rhit4  %rhit5  %rhit6  %rhit7
	 * %whit0  %whit1  %whit2  %whit3  %whit4  %whit5  %whit6  %whit7
	 * %ohit0  %ohit1  %ohit2  %ohit3  %ohit4  %ohit5  %ohit6  %ohit7
	 */
	total_cgroup = iotrack->delta_ios[IOT_NR];
	for (i = 0; i < IOT_NR; i++) {
		for (j = 0; j < LAT_BUCKET_NR; j++) {
			delta = (float)(now->hit[i][j] - last->hit[i][j]);
			iotrack->hit_rate[i][j] = total_cgroup > 0 ?
					100.0 * delta / total_cgroup : 0.0;
			total_hit[j] += delta;
		}
	}

	/* %hit0  %hit1  %hit2  %hit3  %hit4  %hit5  %hit6  %hit7 */
	for (j = 0; j < LAT_BUCKET_NR; j++)
		iotrack->hit_rate[IOT_NR][j] = total_cgroup > 0 ?
			100.0 * total_hit[j] / total_cgroup : 0.0;
}

static void block_gq_show_data(struct block_gq *gq)
{
	struct blk_iotrack *iotrack = &gq->iotrack;
	char buf[LINE_MAX], *p, *e;
	int i;

	memset(buf, 0, sizeof(buf));
	p = buf;
	e = buf + sizeof(buf);
	p += snprintf(p, e - p,
		"%-24s %-16s "		/* timestamp, name */
		,g_ts, gq->dev->name
		);

	if (g_extend)
		p += snprintf(p, e - p,
			"%8.2f %8.2f %8.2f "	/* mean(u.2f) min(u.2f) max(u.2f) */
			,(float)iotrack->stat[g_index].mean	/ 1000.0
			,(float)iotrack->stat[g_index].min	/ 1000.0
			,(float)iotrack->stat[g_index].max	/ 1000.0
			);
	/* io/s */
	p += snprintf(p, e - p, "%8.2f ", iotrack->iops[IOT_NR]);
	/* rio/s wio/s oio/s */
	if (g_extend) {
		/* do not use for loop here, avoid new inserted IOT_XXX */
		p += snprintf(p, e - p,
			"%8.2f %8.2f %8.2f "
			, iotrack->iops[IOT_READ]
			, iotrack->iops[IOT_WRITE]
			, iotrack->iops[IOT_OTHER]
			);
	}

	/* MB/s */
	p += snprintf(p, e - p, "%8.2f ", iotrack->bps[IOT_NR]);
	/* rMB/s wMB/s oMB/s */
	if (g_extend) {
		/* do not use for loop here, avoid new inserted IOT_XXX */
		p += snprintf(p, e - p,
			"%8.2f %8.2f %8.2f "
			, iotrack->bps[IOT_READ]
			, iotrack->bps[IOT_WRITE]
			, iotrack->bps[IOT_OTHER]
			);
	}

	/* %io */
	p += snprintf(p, e - p, "%8.2f ", iotrack->io_pct[IOT_NR]);
	/* %rio %wio %oio */
	if (g_extend) {
		/* do not use for loop here, avoid new inserted IOT_XXX */
		p += snprintf(p, e - p,
			"%8.2f %8.2f %8.2f "
			, iotrack->io_pct[IOT_READ]
			, iotrack->io_pct[IOT_WRITE]
			, iotrack->io_pct[IOT_OTHER]
			);
	}

	/* %MB */
	p += snprintf(p, e - p, "%8.2f ", iotrack->b_pct[IOT_NR]);
	/* %rMB %wMB %oMB */
	if (g_extend) {
		/* do not use for loop here, avoid new inserted IOT_XXX */
		p += snprintf(p, e - p,
			"%8.2f %8.2f %8.2f "
			, iotrack->b_pct[IOT_READ]
			, iotrack->b_pct[IOT_WRITE]
			, iotrack->b_pct[IOT_OTHER]
			);
	}

	/* %tm */
	p += snprintf(p, e - p, "%8.2f ", iotrack->tm_pct[IOT_NR]);
	/* %rtm %wtm %otm */
	if (g_extend) {
		/* do not use for loop here, avoid new inserted IOT_XXX */
		p += snprintf(p, e - p,
			"%8.2f %8.2f %8.2f "
			, iotrack->tm_pct[IOT_READ]
			, iotrack->tm_pct[IOT_WRITE]
			, iotrack->tm_pct[IOT_OTHER]
			);
	}

	/* %dtm */
	p += snprintf(p, e - p, "%8.2f ", iotrack->dtm_pct[IOT_NR]);
	/* %rdtm %wdtm %odtm */
	if (g_extend) {
		/* do not use for loop here, avoid new inserted IOT_XXX */
		p += snprintf(p, e - p,
			"%8.2f %8.2f %8.2f "
			, iotrack->dtm_pct[IOT_READ]
			, iotrack->dtm_pct[IOT_WRITE]
			, iotrack->dtm_pct[IOT_OTHER]
			);
	}

	/* %d2c */
	p += snprintf(p, e - p, "%8.2f ", iotrack->d2c_pct[IOT_NR]);
	/* %rd2c %wd2c %od2c */
	if (g_extend) {
		/* do not use for loop here, avoid new inserted IOT_XXX */
		p += snprintf(p, e - p,
			"%8.2f %8.2f %8.2f "
			, iotrack->d2c_pct[IOT_READ]
			, iotrack->d2c_pct[IOT_WRITE]
			, iotrack->d2c_pct[IOT_OTHER]
			);
	}

	/* %hit0 %hit1  %hit2  %hit3  %hit4  %hit5  %hit6  %hit7 */
	for (i = 0; i < LAT_BUCKET_NR; i++)
		p += snprintf(p, e - p, "%8.2f ", iotrack->hit_rate[IOT_NR][i]);

	/*
	 * %rhit0  %rhit1  %rhit2  %rhit3  %rhit4  %rhit5  %rhit6  %rhit7
	 * %whit0  %whit1  %whit2  %whit3  %whit4  %whit5  %whit6  %whit7
	 * %ohit0  %ohit1  %ohit2  %ohit3  %ohit4  %ohit5  %ohit6  %ohit7
	 */
	if (g_extend) {
		/* rhit */
		for (i = 0; i < LAT_BUCKET_NR; i++)
			p += snprintf(p, e - p, "%8.2f ",
				iotrack->hit_rate[IOT_READ][i]);
		/* rhit */
		for (i = 0; i < LAT_BUCKET_NR; i++)
			p += snprintf(p, e - p, "%8.2f ",
				iotrack->hit_rate[IOT_WRITE][i]);
		/* rhit */
		for (i = 0; i < LAT_BUCKET_NR; i++)
			p += snprintf(p, e - p, "%8.2f ",
				iotrack->hit_rate[IOT_OTHER][i]);
	}

	if (gq->grp->path[0] == '/')
		p += snprintf(p, e - p, "%s\n", gq->grp->path);
	else
		p += snprintf(p, e - p, "/%s\n", gq->grp->path);

	fprintf(stderr, "%s", buf);
}

static int block_cgroup_init_one(struct block_cgroup *g)
{
	char file[PATH_MAX];
	FILE *fp;
	int ret, len = (int)sizeof(file);

	/* block_cgroup second stage initialization */
	INIT_LIST_HEAD(&g->gq_head);

	/*
	 * try to open file for cgroupv1 and v2
	 * cgroup v1: blkio.iotrack.stat
	 * cgroup v2: io.iotrack.stat
	 *
	 */
	ret = snprintf(file, sizeof(file), "%s/%s/%s",
		g_block_cgroup_root_path, g->path, IOTRACK_STAT_FILE_V1);
	if (ret >= len) {
		log("file path is too long %s\n", file);
		return -1;
	}

	fp = fopen(file, "r");

	/* if failed to open blkio.iotrack.stat, try io.iotrack.stat */
	if (!fp) {
		ret = snprintf(file, sizeof(file), "%s/%s/%s",
			g_block_cgroup_root_path, g->path, IOTRACK_STAT_FILE_V2);
		if (ret >= len) {
			log("file path is too long %s\n", file);
			return -1;
		}

		fp = fopen(file, "r");
	}

	if (!fp) {
		log("failed to open %s\n", file);
		return -1;
	}

	g->fp_iotrack_stat = fp;

	if (setvbuf(fp, NULL, _IONBF, 0)) {
		log("failed to setvbuf %s\n", file);
		goto close;
	}

	/* block_cgroup - block_device pair init */
	if (block_gq_init(g))
		goto close;

	g->init_done = 1;

	return 0;

close:
	fclose(fp);
	return -1;
}

static int block_cgroup_init(void)
{
	struct block_cgroup *g, *tmp;

	/* always monitor root block cgroup */
	if (block_cgroup_alloc_one("/", true))
		return -1;

	list_for_each_entry_safe(g, tmp, &g_block_cgroup, node) {
		/*
		 * if a cgroup init failed, just ignore it and remove it from
		 * global block_cgroup list.
		 */
		if (block_cgroup_init_one(g)) {
			block_cgroup_free_one(g);
		}
	}

	return 0;
}

static void block_cgroup_deinit_one(struct block_cgroup *g)
{
	if (g->init_done == 0)
		return;

	block_gq_deinit(g);
	fclose(g->fp_iotrack_stat);
}

static void block_cgroup_deinit(void)
{
	struct block_cgroup *g;

	list_for_each_entry(g, &g_block_cgroup, node)
		block_cgroup_deinit_one(g);
}

static int block_cgroup_read_iostat_one(struct block_cgroup *g)
{
	struct block_gq *gq;
	struct iotrack_stat s;
	char buf[LINE_MAX];
	int nr;

	memset(&s, 0, sizeof(s));
	memset(buf, 0, sizeof(buf));

	rewind(g->fp_iotrack_stat);
	/* read blkio.iotrack.stat */
	while (fgets(buf, sizeof(buf), g->fp_iotrack_stat)) {
		nr = sscanf(buf,
			"%d:%d mean: %llu min: %llu max: %llu sum: %llu "
			"rios: %llu wios: %llu oios:%llu "
			"rsts: %llu wsts: %llu osts: %llu "
			"rtms: %llu wtms: %llu otms: %llu "
			"rdtms: %llu wdtms: %llu odtms: %llu "
			"rhit: %llu %llu %llu %llu %llu %llu %llu %llu "
			"whit: %llu %llu %llu %llu %llu %llu %llu %llu "
			"ohit: %llu %llu %llu %llu %llu %llu %llu %llu\n"
			,
			&s.major, &s.minor, &s.mean, &s.min, &s.max, &s.sum,
			&s.ios[IOT_READ], &s.ios[IOT_WRITE], &s.ios[IOT_OTHER],
			&s.sts[IOT_READ], &s.sts[IOT_WRITE], &s.sts[IOT_OTHER],
			&s.tms[IOT_READ], &s.tms[IOT_WRITE], &s.tms[IOT_OTHER],
			&s.dtms[IOT_READ], &s.dtms[IOT_WRITE], &s.dtms[IOT_OTHER],
			&s.hit[IOT_READ][0], &s.hit[IOT_READ][1],
			&s.hit[IOT_READ][2], &s.hit[IOT_READ][3],
			&s.hit[IOT_READ][4], &s.hit[IOT_READ][5],
			&s.hit[IOT_READ][6], &s.hit[IOT_READ][7],
			&s.hit[IOT_WRITE][0], &s.hit[IOT_WRITE][1],
			&s.hit[IOT_WRITE][2], &s.hit[IOT_WRITE][3],
			&s.hit[IOT_WRITE][4], &s.hit[IOT_WRITE][5],
			&s.hit[IOT_WRITE][6], &s.hit[IOT_WRITE][7],
			&s.hit[IOT_OTHER][0], &s.hit[IOT_OTHER][1],
			&s.hit[IOT_OTHER][2], &s.hit[IOT_OTHER][3],
			&s.hit[IOT_OTHER][4], &s.hit[IOT_OTHER][5],
			&s.hit[IOT_OTHER][6], &s.hit[IOT_OTHER][7]
			);
		if (nr != IOTRACK_STAT_FIELD_NR) {
			log("iotrack stat filed not match %d, expect %d, buf:%s\n",
				nr, IOTRACK_STAT_FIELD_NR, buf);
			return -1;
		}

		/*
		 * lookup block_gq by major:minor, if we can not find it,
		 * that means a new block device has been inserted to the
		 * system, handle this case in the future.
		 */
		gq = block_gq_lookup(g, s.major, s.minor);
		if (!gq) {
			dbg("Not found block_gq major=%d, minor=%d\n",
				s.major, s.minor);
			continue;
		}

		gq->iotrack.stat[g_index] = s;
	}

	return 0;
}

static int block_cgroup_read_iostat(void)
{
	struct block_cgroup *g, *tmp;

	list_for_each_entry_safe(g, tmp, &g_block_cgroup, node) {
		if(block_cgroup_read_iostat_one(g)) {
			if (g == g_block_cgroup_root)
				return -1;
			block_cgroup_deinit_one(g);
			block_cgroup_free_one(g);
		}
	}

	return 0;
}

static int block_cgroup_calc_iostat_one(struct block_cgroup *g)
{
	struct block_gq *gq;

	list_for_each_entry(gq, &g->gq_head, node)
		block_gq_calc_data(gq);

	return 0;
}

static void block_cgroup_calc_iostat(void)
{
	struct block_cgroup *g;

	list_for_each_entry(g, &g_block_cgroup, node) {
		block_cgroup_calc_iostat_one(g);
	}
}

static inline void block_cgroup_show_header(void)
{
	char buf[LINE_MAX], *p, *e;
	if (g_loop && g_loop % HEADER_PER_DATA != 0)
		return;

	memset(buf, 0, sizeof(buf));
	p = buf;
	e = buf + sizeof(buf);
	p += snprintf(p, e - p,
		"%-24s %-16s "		/* timestamp, name */
		, "Time", "Device");
	if (g_extend)
		p += snprintf(p, e - p,
			"%8s %8s %8s "		/* mean(us) min(us) max(us) */
			, "mean(us)", "min(us)", "max(us)");
	/* io/s */
	p += snprintf(p, e - p, "%8s ", "io/s");
	/* rio/s wio/s oio/s */
	if (g_extend)
		p += snprintf(p, e - p,
			"%8s %8s %8s ", "rio/s", "wio/s", "oio/s");

	/* MB/s */
	p += snprintf(p, e - p, "%8s ", "MB/s");
	/* rMB/s wMB/s oMB/s */
	if (g_extend)
		p += snprintf(p, e - p,
			"%8s %8s %8s ", "rMB/s", "wMB/s", "oMB/s");

	/* %io */
	p += snprintf(p, e - p, "%8s ", "%io");
	/* %rio %wio %oio */
	if (g_extend)
		p += snprintf(p, e - p,
			"%8s %8s %8s ", "%rio", "%wio", "%oio");

	/* %MB */
	p += snprintf(p, e - p, "%8s ", "%MB");
	/* %rMB %wMB %oMB */
	if (g_extend)
		p += snprintf(p, e - p,
			"%8s %8s %8s ", "%rMB", "%wMB", "%oMB");

	/* %tm */
	p += snprintf(p, e - p, "%8s ", "%tm");
	/* %rtm %wtm %otm */
	if (g_extend)
		p += snprintf(p, e - p,
			"%8s %8s %8s ", "%rtm", "%wtm", "%otm");

	/* %dtm */
	p += snprintf(p, e - p, "%8s ", "%dtm");
	/* %rtm %wtm %otm */
	if (g_extend)
		p += snprintf(p, e - p,
			"%8s %8s %8s ", "%rdtm", "%wdtm", "%odtm");

	/* %d2c */
	p += snprintf(p, e - p, "%8s ", "%d2c");
	/* %rd2c %wd2c %od2c */
	if (g_extend)
		p += snprintf(p, e - p,
			"%8s %8s %8s ", "%rd2c", "%wd2c", "%od2c");

	/* %hit0 %hit1  %hit2  %hit3  %hit4  %hit5  %hit6  %hit7 */
	p += snprintf(p, e - p,
		"%8s %8s %8s %8s %8s %8s %8s %8s "
		, "%hit0", "%hit1", "%hit2", "%hit3"
		, "%hit4", "%hit5", "%hit6", "%hit7"
		);

	/*
	 * %rhit0  %rhit1  %rhit2  %rhit3  %rhit4  %rhit5  %rhit6  %rhit7
	 * %whit0  %whit1  %whit2  %whit3  %whit4  %whit5  %whit6  %whit7
	 * %ohit0  %ohit1  %ohit2  %ohit3  %ohit4  %ohit5  %ohit6  %ohit7
	 */
	if (g_extend)
		p += snprintf(p, e - p,
			"%8s %8s %8s %8s %8s %8s %8s %8s "
			"%8s %8s %8s %8s %8s %8s %8s %8s "
			"%8s %8s %8s %8s %8s %8s %8s %8s "
			, "%rhit0", "%rhit1", "%rhit2", "%rhit3"
			, "%rhit4", "%rhit5", "%rhit6", "%rhit7"
			, "%whit0", "%whit1", "%whit2", "%whit3"
			, "%whit4", "%whit5", "%whit6", "%whit7"
			, "%ohit0", "%ohit1", "%ohit2", "%ohit3"
			, "%ohit4", "%ohit5", "%ohit6", "%ohit7"
			);
	/* cgroup */
	p += snprintf(p, e - p, "cgroup\n");

	fprintf(stderr, "%s", buf);
}

static void block_cgroup_show_iostat_one(struct block_cgroup *g)
{
	struct block_gq *gq;

	list_for_each_entry(gq, &g->gq_head, node)
		block_gq_show_data(gq);
}


static void block_cgroup_show_iostat(void)
{
	struct block_cgroup *g;

	block_cgroup_show_header();

	list_for_each_entry(g, &g_block_cgroup, node) {
		block_cgroup_show_iostat_one(g);
	}
}

static int read_data()
{
	if (block_device_read_iostat())
		return -1;

	if (block_cgroup_read_iostat())
		return -1;

	return 0;
}

static void calc_data()
{
	block_device_calc_iostat();
	block_cgroup_calc_iostat();
}

static int show_data()
{
	struct tm *tm;
	struct timespec t;
	
	clock_gettime(CLOCK_REALTIME, &t);
	tm = localtime(&t.tv_sec);
	if (!strftime(g_ts, sizeof(g_ts), "%Y-%m-%d %H:%M:%S", tm)) {
		fprintf(stderr, "failed to format time\n");
		return -1;;
	}

	sprintf(g_ts + strlen(g_ts), ".%03lu", t.tv_nsec/1000000);

	/* show block_device level data */
	block_device_show_iostat();
	fprintf(stderr, "\n");

	/* show block_gq level data */
	block_cgroup_show_iostat();
	fprintf(stderr, "\n");

	return 0;
}

static void cleanup_all(void)
{
	block_cgroup_deinit();
	block_cgroup_free();
	block_device_deinit();
}

static void usage(void)
{
	fprintf(stderr, "%s [-r root_cgroup] [-g cgroup] [-x] [-i interval_ms] [-D] [-d disk]\n", g_name);
	fprintf(stderr, "%s  -h  --help: show this help\n", g_name);
	fprintf(stderr, "    -x, --extend: enable extend fields, the extend field show the statistics for read,write and others\n");
	fprintf(stderr, "    -D, --debug: enable debug log\n");
	fprintf(stderr, "    -d disk, --device disk: specify the disk name, i.e. -d nvme0n1 -d sda\n");
	fprintf(stderr, "    -i interval_ms, --interval interval_ms: specify the sampling interval\n");
	fprintf(stderr, "    -r root_cgroup, --root root_cgroup: specify the root block cgroup, i,e. /sys/fs/cgroup/blkio/\n");
}

static int set_interval(const char *arg)
{
	unsigned long long v;

	if (1 != sscanf(arg, "%llu", &v))
		return -1;

	g_delta_time_ms = v;

	return 0;
}

static struct option g_option[] = {
	{"cgroup",	required_argument,	0, 'g'},
	{"extend",	no_argument,		0, 'x'},
	{"interval",	required_argument,	0, 'i'},
	{"debug",	no_argument,		0, 'D'},
	{"device",	required_argument,	0, 'd'},
	{"root",	required_argument,	0, 'r'},
	{"help",	no_argument,		0, 'h'},
	{0, 0, 0, 0}
};

static int parse_args(int argc, char **argv)
{
	int opt, index;

	g_name = argv[0];

	while ((opt = getopt_long(argc, argv, "r:d:g:i:xhD", g_option, &index)) != -1) {
		switch (opt) {
		case 'g':
			if (block_cgroup_alloc_one(optarg, false))
				goto out;
			break;
		case 'd':
			if (block_device_init_one(optarg))
				goto out;
			break;
		case 'r':
			block_cgroup_root_path_init(optarg);
			break;
		case 'x':
			g_extend = 1;
			break;
		case 'D':
			g_debug = 1;
			break;
		case 'i':
			if (set_interval(optarg))
				goto out;
			break;
		case 'h':
			goto out;
		default:
			goto out;
		}
	}

	return 0;

out:
	usage();
	cleanup_all();
	return -1;
}

int main(int argc, char **argv)
{
	if (parse_args(argc, argv))
		goto cleanup;

	if (block_device_init())
		goto cleanup;

	if (block_cgroup_init())
		goto cleanup;

	for (;;) {
		/* read data */
		if (read_data())
			break;

		/* calc data */
		calc_data();

		/* show data */
		if(show_data())
			break;

		/* switch index */
		g_index = 1 - g_index;

		/* sleep interval */
		usleep(g_delta_time_ms * 1000);

		g_loop++;
	}

	block_device_deinit();

	return 0;
cleanup:
	cleanup_all();
	return -1;
}
