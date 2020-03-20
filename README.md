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
	iotrack [-r root_cgroup] [-g cgroup] [-x] [-i interval_ms] [-D] [-d disk]

	-r root_cgroup, --root root_cgroup: specify the root block cgroup path,
		iotrack work with /sys/fs/cgroup/blkio/ by default, if user use cgroup2 or
		mount cgroup1 on other path, just use -r aruments, like:
		iotrack -r /sys/fs/cgroup/unified for cgroup2 in fedora os.

	-g cgroup, --cgroup cgroup: monitor a cgroup's io statistics.
			If you want to monitor multipe cgroup, you can use this option multiple times, i.e.
			$ iotrack -g /test1/ -g /test2

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
		tm = time_complete - time_queue.
		Compare to blktrace, it nearly stands for Q2C.

	%dtm: the cgroup's io total disk time percentile at disk level.
		It's different from "%tm".
		dtm = time_complete - time_driver.
		You can just thinking it's disk hardware time, even it includes irq service time.
		Compare to blktrace, it stands for the time D2C.

	%d2c: the percentile of d2c of q2c, delta_io_disk_time/delta_io_time.

	%hit0: the perctile of the number of io's latency less than
		latency_bucket[0]'s threshold at cgroup level.

		Each cgroup can has its own latency bucket threshold,
		now blk-iotrack provide 8 latency buckets, you check or change
		them by read or write blkio.iotrack.lat_thresh in /.

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
		 0.00    50.00   100.00   100.00   100.00   100.00   100.00   100.00 /test1/

	%hit1: same as %hit0 with different threshold.
	%hit2: same as %hit0 with different threshold.
	%hit3: same as %hit0 with different threshold.
	%hit4: same as %hit0 with different threshold.
	%hit5: same as %hit0 with different threshold.
	%hit6: same as %hit0 with different threshold.
	%hit7: same as %hit0 with different threshold.

	cgroup: the cgroup path

#### Extend field
	mean(us): the mean(average) io latency since this cgroup created.

	min(us): the minimum io latency since this cgroup created.

	max(us): the maximun io latency since this cgroup created.

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

	%rtm: the cgroup's read io time(Q2C) percentile at disk level.

	%wtm: the cgroup's write io time(Q2C) percentile at disk level.

	%otm: the cgroup's other io time(Q2C) percentile at disk level.

	%rdtm: the cgroup's read io time on disk(D2C) percentile at disk level.

	%wdtm: the cgroup's write io time on disk(D2C) percentile at disk level.

	%odtm: the cgroup's other io time on disk(D2C) percentile at disk level.

	%rd2c: the percentile of d2c of q2c for cgroup read io.

	%wd2c: the percentile of d2c of q2c for cgroup write io.

	%od2c: the percentile of d2c of q2c for cgroup others io.

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

#### sample1
test1: bfq weight = 800
test2: bfq weigth = 100
randread 4K

	Time                     Device             rrqm/s   wrqm/s      r/s      w/s    rMB/s    wMB/s  avgrqkb avgqu-sz    await  r_await  w_await    svctm    %util     conc
	2020-03-21 03:08:15.650  nvme1n1              2.00     0.00 44580.00     0.00   174.16     0.00     4.00     5.84     0.24     0.24     0.00     0.00   100.30     5.82

	Time                     Device               io/s     MB/s      %io      %MB      %tm     %dtm     %d2c    %hit0    %hit1    %hit2    %hit3    %hit4    %hit5    %hit6    %hit7 cgroup
	2020-03-21 03:08:15.650  nvme1n1          44588.00   174.17   100.00   100.00   100.00   100.00    38.46     0.25    45.27    95.90    98.33    99.47    99.85    99.92    99.95 /
	2020-03-21 03:08:15.650  nvme1n1          30206.00   117.99    67.74    67.74    29.44    67.29    87.90     0.35    47.82    99.22    99.98    99.99    99.99   100.00   100.00 test1
	2020-03-21 03:08:15.650  nvme1n1          14370.00    56.13    32.23    32.23    70.55    32.69    17.82     0.03    39.89    88.92    94.88    98.37    99.53    99.77    99.85 test2


#### sample2
track test1,test1 for all disk


	Time                     Device             rrqm/s   wrqm/s      r/s      w/s    rMB/s    wMB/s  avgrqkb avgqu-sz    await  r_await  w_await    svctm    %util     conc
	2020-03-21 02:37:33.801  nvme0n1              0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	2020-03-21 02:37:33.801  nvme1n1              0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	2020-03-21 02:37:33.801  sda                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	2020-03-21 02:37:33.801  sdb                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00

	Time                     Device               io/s     MB/s      %io      %MB      %tm     %dtm     %d2c    %hit0    %hit1    %hit2    %hit3    %hit4    %hit5    %hit6    %hit7 cgroup
	2020-03-21 02:37:33.801  nvme0n1              0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /
	2020-03-21 02:37:33.801  nvme1n1              0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /
	2020-03-21 02:37:33.801  sda                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /
	2020-03-21 02:37:33.801  sdb                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 /
	2020-03-21 02:37:33.801  nvme0n1              0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 test1
	2020-03-21 02:37:33.801  nvme1n1              0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 test1
	2020-03-21 02:37:33.801  sda                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 test1
	2020-03-21 02:37:33.801  sdb                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 test1
	2020-03-21 02:37:33.801  nvme0n1              0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 test2
	2020-03-21 02:37:33.801  nvme1n1              0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 test2
	2020-03-21 02:37:33.801  sda                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 test2
	2020-03-21 02:37:33.801  sdb                  0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00 test2
