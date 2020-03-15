# iotrack

Tracking io statistics base on kernel/block/blk-iotrack.

* Collect disk level io statistics
	* nearly same as iostat with an extra `conc` field.
	* Support 8 configurable latency buckets in block cgroup `blkio.iotrack.lat_thresh`.
	* Collect disk's io latency distribution at each latency threshold.
* Collect block cgroup level various io statistics base on kernel/block/blk-iotrack.
	* Collect cgroup's io latency distribution at each latency threshold.
	* Collect cgroup io percentile of whole disk, for read, write, other and all.
	* Collect io/s, MB/s for read, write, other and all.
	* Collect min, mean, max io latency for this cgroup.


You enable blk-iotrack by set  `BLK_CGROUP_IOTRACK=y` in your kernel configuration.

## Arguments
	iotrack [-g cgroup] [-x] [-i interval_ms] [-D] [-d disk]

	-g cgroup, --cgroup cgroup: monitor a cgroup's io statistics.
			If you want to monitor multipe cgroup, you can use this option multiple times, i.e.
			$ iotrack -g /sys/fs/cgroup/blkio/test1/ -g /sys/fs/cgroup/blkio/test2

	-x, --extend: show extend fields, which show the detail statistics for read,write and others.

	-h, --help: show this help.

	-D, --debug: show debug log.

	-d disk, --device disk: specify the disk name, i.e. $ iotrack -d nvme0n1 -d sda -d sdb

	-i interval_ms, --interval interval_ms: specify the sampling interval in unit of millisecond.


## Output field
iotrack now collects two level data, block_device and block_cgroup.

### Common field
	Time: yyyy-mm-dd HH:MM:SS.sss

	Device: block device name

### block_device
It adds one more field *conc* which stands for io concurrency.

	rrqm/s: read request merge count.

	wrqm/s: write request merge count.

	r/s: read reqeust per second.

	w/s: write request per second.

	rMB/s: read data(Mibibyte) per second.

	wMB/s: write data(Mibibyte) per second.

	avgrqkb: average request in unit of KiByte.

	avgqu-sz: average reqeust queue size.

	await: average wait time for both read and write io.

	r_await: average wait time for both read.

	w_await:  average wait time for both write.

	svctm: always 0.

	%util: disk io time in sampling time window.

	conc: io concurrency. time_in_queue/io_ticks.

### block_cgroup
It collect lots data from blkio.iotrack.stat, it contains two parts data.

The basic field show the overal io statistics include both read, write and flush,discard, etc.

The extend fileds show io statistics sperately for read, write, and others.

#### Basic field
	mean(us): the mean(average) io latency since this cgroup created.

	min(us): the minimum io latency since this cgroup created.

	max(us): the maximun io latency since this cgroup created.

	io/s: io per second, include read, write and others.

	MB/s: Mibibyte per second,include read, write and others.

	%io: the cgroup's io number percentile at disk level.
		%io = cgroup.disk.ios/disk.ios.
		Example: disk totally complete 100 io, and cgroup1 complete 20 io,
		the %io = 20 / 100 = 20%.

	%MB: the cgroup's io data(Mibibyte) percentile at disk level, same as
			"%io".

	%tm: the cgroup's io total time percentile at disk level.
		%tm = cgroup.disk.io_time_sum / disk.io_time_sum.
		The io_time_sum stands for the sum of all completed io.

	%hit0: the perctile of the number of io's latency less than
		latency_bucket[0]'s threshold at cgroup level.

		Each cgroup can has its own latency bucket threshold,
		now blk-iotrack provide 8 latency buckets, you check or change
		them by read or write blkio.iotrack.lat_thresh in /sys/fs/cgroup/blkio/.

		The default latency bucket threshold:
		bucket[0] = 50	us
		bucket[1] = 100	us
		bucket[2] = 200	us
		bucket[3] = 400	us
		bucket[4] = 1	ms
		bucket[5] = 2	ms
		bucket[6] = 4	ms
		bucket[7] = 8	ms

		Example:
		%hit0    %hit1    %hit2    %hit3    %hit4    %hit5    %hit6    %hit7 cgroup
		---------------------------------------------------------------------------
		 0.00    50.00   100.00   100.00   100.00   100.00   100.00   100.00 /sys/fs/cgroup/blkio/test1/

	%hit1: same as %hit0 with different threshold.
	%hit2: same as %hit0 with different threshold.
	%hit3: same as %hit0 with different threshold.
	%hit4: same as %hit0 with different threshold.
	%hit5: same as %hit0 with different threshold.
	%hit6: same as %hit0 with different threshold.
	%hit7: same as %hit0 with different threshold.

	cgroup: the cgroup path

#### Extend field
	rio/s: read io per secons.

	wio/s: write io per secons.

	oio/s: other io per secons, like flush, discard.

	rMB/s: read data(Mibibyte) per second.

	wMB/s: write data(Mibibyte) per second.

	oMB/s: other data(Mibibyte) per second, like flush, discard.

	%rio: the cgroup's read io number percentile at disk level.

	%wio: the cgroup's write io number percentile at disk level.

	%oio: the cgroup's other io number percentile at disk level, like flush, discard.

	%rMB: the cgroup's read io data(Mibibyte) percentile at disk level.

	%wMB: the cgroup's write io data(Mibibyte) percentile at disk level.

	%oMB: the cgroup's other io data(Mibibyte) percentile at disk level, like flush, discard.

	%rhit0: the perctile of the number of read io's latency less than
		latency_bucket[0]'s threshold at cgroup level.

	%rhit1: same as %rhit0 with different threshold.
	%rhit2: same as %rhit0 with different threshold.
	%rhit3: same as %rhit0 with different threshold.
	%rhit4: same as %rhit0 with different threshold.
	%rhit5: same as %rhit0 with different threshold.
	%rhit6: same as %rhit0 with different threshold.
	%rhit7: same as %rhit0 with different threshold.

	%whit0: the perctile of the number of write io's latency less than
		latency_bucket[0]'s threshold at cgroup level.

	%whit1: same as %whit0 with different threshold.
	%whit2: same as %whit0 with different threshold.
	%whit3: same as %whit0 with different threshold.
	%whit4: same as %whit0 with different threshold.
	%whit5: same as %whit0 with different threshold.
	%whit6: same as %whit0 with different threshold.
	%whit7: same as %whit0 with different threshold.

	%ohit0: the perctile of the number of other(flush,discard) io's latency less than
		latency_bucket[0]'s threshold at cgroup level.

	%ohit1: same as %ohit0 with different threshold.
	%ohit2: same as %ohit0 with different threshold.
	%ohit3: same as %ohit0 with different threshold.
	%ohit4: same as %ohit0 with different threshold.
	%ohit5: same as %ohit0 with different threshold.
	%ohit6: same as %ohit0 with different threshold.
	%ohit7: same as %ohit0 with different threshold.


### Sample
	Time                     Device             rrqm/s   wrqm/s      r/s      w/s    rMB/s    wMB/s  avgrqkb avgqu-sz    await  r_await  w_await    svctm    %util     conc
	2020-03-13 17:08:24.631  dm-1                 0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	2020-03-13 17:08:24.631  nvme0n1              0.00     0.00  7721.00     0.00   447.26     0.00    59.32     0.03     0.13     0.13     0.00     0.00    99.90     0.03
	2020-03-13 17:08:24.631  sdb                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	2020-03-13 17:08:24.631  dm-2                 0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	2020-03-13 17:08:24.631  dm-0                 0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	2020-03-13 17:08:24.631  nvme1n1              0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	2020-03-13 17:08:24.631  sda                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00

	Time                     Device           mean(us)  min(us)  max(us)     io/s     MB/s      %io      %MB      %tm    %hit0    %hit1    %hit2    %hit3    %hit4    %hit5    %hit6    %hit7 cgroup
	2020-03-13 17:08:24.631  dm-1               391.56    45.81  1520.76     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio
	2020-03-13 17:08:24.631  nvme0n1            160.66     5.64  6086.27  7723.00     0.65   100.00   100.00   111.68    22.83    39.66    60.77    94.67    99.62   100.00   100.00   100.00 /sys/fs/cgroup/blkio
	2020-03-13 17:08:24.631  sdb                192.13    19.02  4925.24     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio
	2020-03-13 17:08:24.631  dm-2               476.65    25.22 18484.16     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio
	2020-03-13 17:08:24.631  dm-0               672.65    18.94 24603.28     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio
	2020-03-13 17:08:24.631  nvme1n1            184.97    49.91   791.02     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio
	2020-03-13 17:08:24.631  sda                589.41    17.09 24587.85     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio
	2020-03-13 17:08:24.631  dm-1                 0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test1
	2020-03-13 17:08:24.631  nvme0n1            157.58     5.89  4503.47  3788.00     0.32    49.05    49.21    47.89    25.26    41.76    62.70    94.98    99.66   100.00   100.00   100.00 /sys/fs/cgroup/blkio/test1
	2020-03-13 17:08:24.631  sdb                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test1
	2020-03-13 17:08:24.631  dm-2                 0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test1
	2020-03-13 17:08:24.631  dm-0               353.93   176.71   675.06     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test1
	2020-03-13 17:08:24.631  nvme1n1              0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test1
	2020-03-13 17:08:24.631  sda                220.18    74.68   671.66     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test1
	2020-03-13 17:08:24.631  dm-1                 0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test2
	2020-03-13 17:08:24.631  nvme0n1            164.16     5.64  6086.27  3935.00     0.33    50.95    50.82    52.10    20.46    37.61    58.93    94.36    99.59   100.00   100.00   100.00 /sys/fs/cgroup/blkio/test2
	2020-03-13 17:08:24.631  sdb                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test2
	2020-03-13 17:08:24.631  dm-2                 0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test2
	2020-03-13 17:08:24.631  dm-0               481.22   459.92   502.53     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test2
	2020-03-13 17:08:24.631  nvme1n1              0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test2
	2020-03-13 17:08:24.631  sda                397.82   298.87   496.77     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /sys/fs/cgroup/blkio/test2
