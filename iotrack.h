#ifndef __IOTRACK_H__
#define __IOTRACK_H__

#define BLOCK_DEVICE_DIR "/sys/block"
#define BLOCK_CGROUP_ROOT "/sys/fs/cgroup/blkio"
#define HEADER_PER_DATA 1

/* align with kernel block/blk-iotrack.c */
#define LAT_BUCKET_NR 8
enum {
	IOT_READ,
	IOT_WRITE,
	IOT_OTHER,
	IOT_NR,
};

#define IOSTAT_FIELD_NR	11
struct iostat {
	unsigned long long r_ios;
	unsigned long long r_merge;
	unsigned long long r_sector;
	unsigned long long r_ticks;
	unsigned long long w_ios;
	unsigned long long w_merge;
	unsigned long long w_sector;
	unsigned long long w_ticks;
	unsigned long long in_flight;
	unsigned long long io_ticks;
	unsigned long long time_in_queue;
};

struct block_device {
	struct list_head node;
	int major;
	int minor;
	char name[128];

	int fd_iostat;
	struct iostat iostat[2];
	float rrqm, wrqm;		/* rrqm/s   wrqm/s */
	float r, w;			/* r/s     w/s */
	float rmb, wmb;			/* rMB/s    wMB/s */
	float avgrqkb, avgqu;		/* avgrqkb avgqu-sz */
	float await, r_await, w_await;	/* await r_await w_await */
	/* svctm=0 */
	float util;			/* io.util */
	float conc;			/* io.conc concurrency */
};

struct block_cgroup;

#define IOTRACK_STAT_FIELD_NR	(2 + IOT_NR * 3 + 4 + IOT_NR * LAT_BUCKET_NR)
struct iotrack_stat {
	int major;
	int minor;

	/* latency ns */
	unsigned long long mean;
	unsigned long long min;
	unsigned long long max;
	unsigned long long sum;

	/* completed io, kb and latency_sum */
	unsigned long long ios[IOT_NR];
	unsigned long long kbs[IOT_NR];
	unsigned long long tms[IOT_NR];

	/* latency distribution */
	unsigned long long hit[IOT_NR][LAT_BUCKET_NR];
};

struct blk_iotrack {
	struct iotrack_stat stat[2];

	/*
	 * very usefull temp data:
	 * escpecially for root block_gq, it used for calculate all
	 * children's percentile data, i.e. io_pct,b_pct.
	 */
	unsigned long long delta_ios[IOT_NR + 1];	/* r(ead) + w(rite) + o(ther) */
	unsigned long long delta_kbs[IOT_NR + 1];	/* r(ead) + w(rite) + o(ther) */
	unsigned long long delta_tms[IOT_NR + 1];	/* r(ead) + w(rite) + o(ther) */

	float iops[IOT_NR + 1];		/* r/s, w/s, o/s, io/s */
	float bps[IOT_NR + 1];		/* rMB/s, wMB/s, oMB/s, MB/s */
	float io_pct[IOT_NR + 1];	/* cgroup.ios[i] / disk.ios[IOT_NR] */
	float b_pct[IOT_NR + 1];	/* cgroup.kbs[i] / disk.kbs[IOT_NR] */
	float tm_pct[IOT_NR + 1];	/* cgroup.tms[i] / disk.tms[IOT_NR] */
	/*
	 * %rhit, %whit, %ohit, %hit
	 * hit_rate[i][j] = delta_iotrack_stat.hit[i][j] / delta_ios[IOT_NR];
	 */
	float hit_rate[IOT_NR + 1][LAT_BUCKET_NR];
};

/* block_cgroup - block_device pair */
struct block_gq {
	struct list_head node;
	struct block_device *dev;
	struct block_cgroup *grp;

	struct blk_iotrack iotrack;
};

struct block_cgroup {
	struct list_head node;
	struct list_head gq_head;
	char path[PATH_MAX];

	FILE *fp_iotrack_stat;
	int init_done;
};

#endif /* __IOTRACK_H__ */
