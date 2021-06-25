#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <scsi/scsi_ioctl.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#ifndef major
#include <sys/types.h>
#endif
#include <linux/major.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <byteswap.h>

#include <bits/stdc++.h>

#ifndef RAW_MAJOR
#define RAW_MAJOR 255 /*unlikely value */
#endif

enum
{
  kReadOp = 1,
  kWriteOp = 2,
  kBlockSize = 512,
};


/**
 * Utilities can use these exit status values for syntax errors and
 * file (device node) problems (e.g. not found or permissions).
 * command line syntax problem
 **/
#define SG_LIB_SYNTAX_ERROR 1

#define SENSE_BUFF_LEN 64 /* Arbitrary, could be larger */
#define DEF_TIMEOUT 40000 /* 40,000 millisecs == 40 seconds */


#define FT_OTHER 1  /* filetype other than sg and ... */
#define FT_SG 2     /* filetype is sg char device */
#define FT_RAW 4    /* filetype is raw char device */
#define FT_BLOCK 8  /* filetype is block device */
#define FT_ERROR 64 /* couldn't "stat" file */

constexpr uint32_t KiB = 1024;
constexpr uint32_t MiB = KiB * KiB;
constexpr uint32_t k4KiB = KiB << 2;

inline void Sleep(int ms) {
    if (ms <= 0) {
      return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

static std::pair<int, std::string>
file_type_list[] = {
  {FT_OTHER, "FT_OTHER"},
  {FT_SG, "FT_SG"},
  {FT_RAW, "FT_RAW"},
  {FT_BLOCK, "FT_BLOCK"},
  {FT_ERROR, "FT_ERROR"},
};

// 去拿文件的类型
static int get_file_type(const char *filename) {
  struct stat st;

  if (stat(filename, &st) < 0) {
    return FT_ERROR;
  }

  if (S_ISCHR(st.st_mode)) {
    if (RAW_MAJOR == major(st.st_rdev)) {
      return FT_RAW;
    } else if (SCSI_GENERIC_MAJOR == major(st.st_rdev)) {
      return FT_SG;
    }
  } else if (S_ISBLK(st.st_mode)) {
    return FT_BLOCK;
  }
  return FT_OTHER;
}

// 输出文件的类型
static void print_file_type(int flag) {
  const int N = sizeof(file_type_list) / sizeof(*file_type_list);
  for (int i = 0; i < N; i++) {
    const int mask = file_type_list[i].first;
    if (flag & mask) {
      printf("%s | ", file_type_list[i].second.c_str());
    }
  }
  printf("\n");
}


// 文件操作
static const char *file_name = "/dev/sg2";
int fd = -1;


// 写入之后是否要检查
static bool do_verify = false;


#define VERIFY10 0x2f
#define VERIFY12 0xaf
#define VERIFY16 0x8f


static inline void sg_put_unaligned_be16(uint16_t val, void *p) {
        uint16_t u = bswap_16(val);
        memcpy(p, &u, 2);
}

static inline void sg_put_unaligned_be32(uint32_t val, void *p) {
        uint32_t u = bswap_32(val);
        memcpy(p, &u, 4);
}

// 开始构建SCSI命令!
static int
sg_build_scsi_cdb(uint8_t *cdbp,
                  int cdb_sz,
                  unsigned int blocks,
                  int64_t start_block,
                  bool is_verify,
                  bool write_true,
                  bool fua,
                  bool dpo)
{
    int sz_ind;
    int rd_opcode[] = {0x8, 0x28, 0xa8, 0x88};
    int ve_opcode[] = {0xff /* no VERIFY(6) */,
                       VERIFY10,
                       VERIFY12,
                       VERIFY16};
    int wr_opcode[] = {0xa, 0x2a, 0xaa, 0x8a};

    memset(cdbp, 0, cdb_sz);

    // 先把检查打开
    assert(is_verify == true);
    // 我们先考虑为10的时候的处理
    assert(cdb_sz == 10);

    cdbp[1] = 0x2;

    is_verify = false;
        if (dpo)
      cdbp[1] |= 0x10;
    if (fua)
      cdbp[1] |= 0x8;

    sz_ind = 1;
    if (is_verify && write_true) {
      cdbp[0] = ve_opcode[sz_ind];
    } else {
      cdbp[0] = (uint8_t)(write_true ? wr_opcode[sz_ind] :
        rd_opcode[sz_ind]);
    }

    sg_put_unaligned_be32(start_block, cdbp + 2);
    sg_put_unaligned_be16(blocks, cdbp + 7);

    if (blocks & (~0xffff)) {
      printf("for 10 byte commands, maximum number of blocks is "
                "%d\n", 0xffff);
      return SG_LIB_SYNTAX_ERROR;
    }

    return 0;
}

/**
 * coe:
 * 0: exit on error (def)
 * 1: continue on sg error (zero fill)
 * 2: also try read_long on unrecovered reads,
 * 3: and set the CORRCT bit on the read long
 **/
static int coe = 0;

/* failed but coe set */
#define SG_DD_BYPASS 999

uint8_t wrCmd[1024];
uint8_t senseBuff[1024];
int pack_id_count;

/**
 *
 * 函数功能：
 *
 *  调用SCSI命令写一个设备!
 *  Does a SCSI WRITE or VERIFY (if do_verify set) on OFILE.
 *
 * 输入参数：
 *
 *    infd:       文件句柄
 *    wrkPos:     内存位置
 *    blocks:     要读的blocks的数目
 *    skip:       从哪里开始读?
 *    bs:         block的大小
 *    scsi_cdbsz: cmd缓冲区的大小
 *    fua:        fua要用吗？
 *    dpo:        device cache要不要被调度出去!
 *    &diop:      diop值有可能被改变
 *
 * 返回值：
 *
 * 0:                           successful
 * SG_LIB_SYNTAX_ERROR:         unable to build cdb,
 * SG_LIB_CAT_NOT_READY:
 * SG_LIB_CAT_UNIT_ATTENTION:
 * SG_LIB_CAT_MEDIUM_HARD:
 * SG_LIB_CAT_ABORTED_COMMAND:
 * -2:                          recoverable (ENOMEM),
 * -1:                          unrecoverable error + others
 * SG_DD_BYPASS:                failed but coe set.
 *
 **/
static int
sg_write(int sg_fd,
         uint8_t *buff,
         int blocks,
         int64_t to_block,
         int bs,
         int cdbsz,
         bool fua,
         bool dpo,
         bool *diop)
{
    bool info_valid;
    int res;
    uint64_t io_addr = 0;
    struct sg_io_hdr io_hdr;

    assert(0 == sg_build_scsi_cdb(wrCmd,
                          cdbsz,
                          blocks,
                          to_block,
                          true/*verify*/,
                          true/*write_true*/,
                          fua,
                          dpo));
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));

    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = cdbsz;
    io_hdr.cmdp = wrCmd;
    io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr.dxfer_len = bs * blocks;
    io_hdr.dxferp = buff;
    io_hdr.mx_sb_len = SENSE_BUFF_LEN;
    io_hdr.sbp = senseBuff;
    io_hdr.timeout = DEF_TIMEOUT;
    io_hdr.pack_id = pack_id_count++;

    if (diop && *diop) {
        io_hdr.flags |= SG_FLAG_DIRECT_IO;
    }

    while (((res = ioctl(sg_fd, SG_IO, &io_hdr)) < 0) &&
           ((EINTR == errno) || (EAGAIN == errno) || (EBUSY == errno))) {
        /*Nothing*/;
    }

    assert(res == 0);

//    if (diop && *diop &&
//        ((io_hdr.info & SG_INFO_DIRECT_IO_MASK)
//          != SG_INFO_DIRECT_IO)) {
//      printf("write NOT over!\n");
//      *diop = false;
//    }

    return 0;
}

// index stands for microsecond
static uint64_t time_stat_microsecond[1000000];
static uint64_t max_io_time_nanosecond = 0;
static uint64_t min_io_nanosecond = INT64_MAX;

// 这里写一下要写多少次
// 为了简单起见，这里我们只写10次
// 也就是发送10次指令
constexpr int64_t total_write_times = 10000;

/**
 * Function:
 *  Used to print out the results of disk performance
 *
 * Arguments:
 *  - IOPS: total iops during the time.
 *  - total_time (nanoseconds): total running time
 *  - unit_size (bytes): every read/write_size
 *
 * output: Just like fio
 *    |  1.00th=[ 1020],  5.00th=[ 1237], 10.00th=[ 1401], 20.00th=[ 7373],
 *    | 30.00th=[ 7701], 40.00th=[ 7832], 50.00th=[ 7898], 60.00th=[ 7963],
 *    | 70.00th=[ 8094], 80.00th=[ 8160], 90.00th=[ 8291], 95.00th=[ 8356],
 *    | 99.00th=[ 8455], 99.50th=[ 8586], 99.90th=[ 8848], 99.95th=[ 8979],
 *    | 99.99th=[ 9110]
 */
static void
output_result(int op_type,
              uint64_t iops,
              uint64_t total_time,
              uint64_t uint_size)
{
  printf("min_time = %.3f (ms)\n", (double)min_io_nanosecond / 1000.0 / 1000.0);
  printf("max_time = %.3f (ms)\n",
         (double)max_io_time_nanosecond / 1000.0 / 1000.0);
  printf("total_iops = %lu\n", iops);
  printf("total_time = %.3f (ms)\n", double(total_time) / 1000.0 / 1000.0);

  static double per[] = { 1,  5,  10, 20, 30,   40,   50,    60,   70,
                          80, 90, 95, 99, 99.5, 99.9, 99.95, 99.99 };

  double per_ret_ms[sizeof(per) / sizeof(*per)];

  if (op_type == kReadOp) {
    std::cout << "Op = "
              << "Read" << std::endl;
  } else {
    std::cout << "Op = "
              << "Write" << std::endl;
  }

  // iops per second
  std::cout << "IOPS = " << (iops * 1000 * 1000 * 1000) / total_time
            << " iops/s" << std::endl;

  // BW (bytes/second)
  std::cout << "BW = "
            << (double)((double)iops * (double)uint_size * 1000 * 1000 * 1000) /
                 (double)total_time / 1024 / 1024
            << " MB/s" << std::endl;

  double cnt = 0;
  double total_item = iops;

  int per_idx = 0;

  // compute ops percentile
  for (uint64_t i = min_io_nanosecond / 1000;
       i <= max_io_time_nanosecond / 1000;
       i++) {
    // want percentile
    if (cnt / total_item * 100.0 <= per[per_idx]) {
      per_ret_ms[per_idx] = (double)i / 1000.0;

      // 防止空洞的情况发生
      if (per_idx + 1 < sizeof(per_ret_ms) / sizeof(*per_ret_ms)) {
        per_ret_ms[per_idx + 1] = (double) i / 1000.0;
      }

    } else {
      per_idx++;
    }
    cnt += time_stat_microsecond[i];
  }

  for (int i = 0; i < sizeof(per) / sizeof(*per); i++) {
    printf("percentile %.2lf %%= %.3lf (ms)\n", per[i], per_ret_ms[i]);
  }
}


int main(int argc, char **argv) {
  auto ret = get_file_type(argv[1]);

  auto open_file_flags = O_RDWR | O_DIRECT;

  fd = open(argv[1], open_file_flags);
  assert(-1 != fd);

  uint8_t *write_buf = nullptr;

  // 申请一个4MB并且4KB对齐的内存块
  assert(0 == posix_memalign((void**)&write_buf,
                              k4KiB,
                              MiB << 2));
  assert(write_buf);
  // 初始化这个内存块
  for (int i = 0; i < (MiB<<2); i++) {
    write_buf[i] = random() % 26 + 'a';
  }

  // 这里我们要开始计时
  auto start_total_time = std::chrono::system_clock::now();

  auto get_diff_ns = [](const decltype(start_total_time)& start,
                        const decltype(start_total_time) end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    // 1,000 nanoseconds – one microsecond
    // 1,000 microseconds - one ms
  };

  int file_fd = open("/tmp/res", O_RDWR | O_CREAT | O_TRUNC);
  assert(file_fd != -1);

  uint64_t time_cost[total_write_times];
  uint64_t time_cost_nanosecond = 0;

  for (int64_t i = 0; i < total_write_times; i++) {
    bool direct_io = false;
    // 到这里我们开始写

    auto start = std::chrono::system_clock::now();

    auto ret = sg_write(fd,
                        write_buf,
                        1/*block per write*/,
                        i/*write_to_block_pos*/,
                        k4KiB/*block_size*/,
                        10/*SCSI cmd length*/,
                        false/*FUA*/,
                        false/*dpo*/,
                        &direct_io);

    auto end = std::chrono::system_clock::now();;

    uint64_t current_io_nanosecond = get_diff_ns(start, end).count();
    max_io_time_nanosecond = std::max(max_io_time_nanosecond, current_io_nanosecond);
    min_io_nanosecond = std::min(min_io_nanosecond, current_io_nanosecond);

    time_cost[i] = current_io_nanosecond;
    time_stat_microsecond[current_io_nanosecond / 1000]++;
    time_cost_nanosecond += current_io_nanosecond;

    assert(ret == 0);
    // 写完为了避免threshold.这里加个延时
    Sleep(3);

    assert(k4KiB == write(file_fd, write_buf, k4KiB));
  }

  auto end_total_time = std::chrono::system_clock::now();

  // 停止计时！

  free(write_buf);
  close(fd);
  close(file_fd);

//  for (int i = 0; i < total_write_times; i++) {
//    std::cout << time_cost[i] << std::endl;
//  }

  output_result(kWriteOp,
                total_write_times,
                time_cost_nanosecond,
                k4KiB);

  return 0;
}
