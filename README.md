# iotrack

Tracking io statistics base on kernel/block/blk-iotrack.

Classicall cgroup file path for blk-iotrack:

	cgroup v1: /sys/fs/cgroup/blkio/blkio.iotrack.stat
	cgroup v2: /sys/fs/cgroup/io.iotrack.stat

* Collect disk level io statistics
	* nearly same as iostat with an extra `conc` field.
	* Support 8 configurable latency buckets in block cgroup `blkio.iotrack.lat_thresh` or `io.iotrack.lat_thresh` for cgroup v2.
	* Collect disk's io latency distribution at each latency threshold.
* Collect block cgroup level various io statistics base on kernel/block/blk-iotrack.
	* Collect cgroup's io latency distribution at each latency threshold.
	* Collect cgroup's %io, %bytes, %dtm of whole disk, for read, write, other and all.
	* Collect io/s, MB/s for read, write, other and all.
	* Collect aq2c average Q2C time for this cgroup.
	* Collect ad2c averarge D2C time for this cgroup.
	* Collect %d2c the percentage of D2C in Q2C, %d2c = d2c/q2c;
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

	rMB/s: read data(bytes) per second.

	wMB/s: write data(bytes) per second.

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

	MB/s: bytes per second,include read, write and others.

	%io: the cgroup's io number percentile at disk level.
		%io = cgroup.disk.ios/disk.ios.
		Example: disk totally complete 100 io, and cgroup1 complete 20 io,
		the %io = 20 / 100 = 20%.

	%byte: the cgroup's io data(bytes) percentile at disk level, same as
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

	ad2c: average io latency(ms) for d2c.

	aq2c: average io latency(ms) for q2c.

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

	rMB/s: read data(bytes) per second.

	wMB/s: write data(bytes) per second.

	oMB/s: other data(bytes) per second, like flush, discard.

	%rio: the cgroup's read io number percentile at disk level.

	%wio: the cgroup's write io number percentile at disk level.

	%oio: the cgroup's other io number percentile at disk level, like flush, discard.

	%rMB: the cgroup's read io data(bytes) percentile at disk level.

	%wMB: the cgroup's write io data(bytes) percentile at disk level.

	%oMB: the cgroup's other io data(bytes) percentile at disk level, like flush, discard.

	%rtm: the cgroup's read io time(Q2C) percentile at disk level.

	%wtm: the cgroup's write io time(Q2C) percentile at disk level.

	%otm: the cgroup's other io time(Q2C) percentile at disk level.

	%rdtm: the cgroup's read io time on disk(D2C) percentile at disk level.

	%wdtm: the cgroup's write io time on disk(D2C) percentile at disk level.

	%odtm: the cgroup's other io time on disk(D2C) percentile at disk level.

	%rd2c: the percentile of d2c of q2c for cgroup read io.

	%wd2c: the percentile of d2c of q2c for cgroup write io.

	%od2c: the percentile of d2c of q2c for cgroup others io.

	rd2c: the average read io latency(ms) for D2C.

	wd2c: the average write io latency(ms) for D2C.

	od2c: the average other io latency(ms) for D2C.

	rq2c: the average read io latency(ms) for Q2C.

	wq2c: the average write io latency(ms) for Q2C.

	oq2c: the average other io latency(ms) for Q2C.

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
	cgroup /test1: bfq weight = 800
	cgroup /test2: bfq weight = 100
	randread 4K numjobs=8 iodepth=32

* The root block cgroup `"/"` shows the io statistics for whole ssd disk.

* test1 use disk's `%88` iops and bps.

* `%dtm` stands for the on disk time, test1 cgroup get `85%` of whole disk,
	that means test1 gets more disk time than test2.

* For test2's `%d2c`, there is only `1.17%` latency cost at hardware disk,
	that means the main latency cames from software, it was
	throttled by softwre.

* test2's aq2c(11ms) > test1's aq2c(1ms).

* For latency distribution, hit1(<=100us), 64% io of test1 <=100us, and
	43% io of test2 <=100us.

	$ iotrack -d nvme1n1 -g /test1 -g /test2

Sample result:

	Device           rrqm/s   wrqm/s   r/s      w/s      rMB/s    wMB/s    avgrqkb  avgqu-sz await    r_await  w_await  svctm    %util    conc
	nvme1n1          0        0        217341   0        848.98   0.00     4.00     475.03   2.28     2.28     0.00     0.00     100.20   474.08

	Device           io/s     MB/s     %io      %byte      %tm      %dtm     %d2c     ad2c     aq2c     %hit0    %hit1    %hit2    %hit3    %hit4    %hit5    %hit6    %hit7    cgroup
	nvme1n1          217345   849.00   100.00   100.00   100.00   100.00   4.09     0.09     2.28     23.97    62.43    89.88    98.44    99.88    99.88    99.88    99.88    /
	nvme1n1          193183   754.62   88.88    88.88    45.91    84.54    7.52     0.09     1.18     26.85    64.87    90.71    98.40    99.88    99.88    99.88    99.88    /test1
	nvme1n1          24235    94.67    11.15    11.15    54.09    15.48    1.17     0.13     11.06    0.98     43.00    83.31    98.77    99.87    99.87    99.87    99.87    /test2

#### sample2

	cgroup /test1: io.weight = 100
	cgroup /test2: io.weight = 100
	randread 4K numjobs=8 iodepth=32
	disable io scheduler: `echo none > /sys/block/nvme1n1/queue/scheduler`

	$ iotrack -d nvme1n1 -g /test1 -g /test2

	Time: 2020-03-27 17:43:51
	Device           rrqm/s   wrqm/s   r/s      w/s      rMB/s    wMB/s    avgrqkb  avgqu-sz await    r_await  w_await  svctm    %util    conc
	nvme1n1          0        0        761246   0        2973.62  0.00     4.00     118.76   0.67     0.67     0.00     0.00     100.20   118.52

	Device           io/s     MB/s     %io    %byte  %tm    %dtm   %d2c   ad2c     aq2c     %hit0  %hit1  %hit2  %hit3  %hit4  %hit5  %hit6  %hit7  cgroup
	nvme1n1          761246   2973.62  100.00 100.00 100.00 100.00 99.91  0.67     0.67     0.00   0.00   0.10   12.56  84.73  99.71  100.00 100.00 /
	nvme1n1          402702   1573.05  52.90  52.90  49.93  49.93  99.90  0.63     0.63     0.00   0.00   0.11   14.12  89.01  99.80  100.00 100.00 /test1
	nvme1n1          358610   1400.82  47.11  47.11  50.08  50.08  99.92  0.71     0.71     0.00   0.00   0.09   10.78  79.93  99.61  100.00 100.00 /test2

#### sample3

	track test1,test1 for all disk

	Device           rrqm/s   wrqm/s   r/s      w/s      rMB/s    wMB/s    avgrqkb  avgqu-sz await    r_await  w_await  svctm    %util    conc
	dm-1             0        0        0        0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	nvme0n1          0        0        0        0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	sdb              0        0        0        0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	dm-2             0        0        0        0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	dm-0             0        0        0        0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	nvme1n1          0        0        0        0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00
	sda              0        0        0        0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00

	Device           io/s     MB/s     %io      %byte    %tm      %dtm     %d2c     ad2c     aq2c     %hit0    %hit1    %hit2    %hit3    %hit4    %hit5    %hit6    %hit7    cgroup
	dm-1             0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /
	nvme0n1          0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /
	sdb              0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /
	dm-2             0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /
	dm-0             0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /
	nvme1n1          0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /
	sda              0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /
	dm-1             0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test1
	nvme0n1          0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test1
	sdb              0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test1
	dm-2             0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test1
	dm-0             0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test1
	nvme1n1          0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test1
	sda              0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test1
	dm-1             0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test2
	nvme0n1          0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test2
	sdb              0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test2
	dm-2             0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test2
	dm-0             0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test2
	nvme1n1          0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test2
	sda              0        0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     0.00     /test2
