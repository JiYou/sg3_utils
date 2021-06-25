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
static const char *file_name = "/dev/sdb";
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

    if (sg_build_scsi_cdb(wrCmd,
                          cdbsz,
                          blocks,
                          to_block,
                          true/*verify*/,
                          true/*write_true*/,
                          fua,
                          dpo)) {
      printf("build command failed\n");
        return SG_LIB_SYNTAX_ERROR;
    }

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

    /* flag that dio not done (completely) */
    if (diop && *diop &&
        ((io_hdr.info & SG_INFO_DIRECT_IO_MASK)
          != SG_INFO_DIRECT_IO)) {
        *diop = false;
    }

    return 0;
}

int main(void) {
  auto ret = get_file_type(file_name);
  assert(FT_BLOCK & ret);

  auto open_file_flags = O_RDWR | O_DIRECT;

  fd = open(file_name, open_file_flags);
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

  // 这里写一下要写多少次
  // 为了简单起见，这里我们只写10次
  // 也就是发送10次指令
  constexpr int64_t total_write_times = 10;

  // 这里我们要开始计时
  auto start_total_time = std::chrono::system_clock::now();

  auto get_diff_ns = [](const decltype(start_total_time)& start,
                        const decltype(start_total_time)& end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    // 1,000 nanoseconds – one microsecond
    // 1,000 microseconds - one ms
  };

  // 记录下每个请求所用的时间
  std::vector<int> time_cost;

  for (int64_t i = 0; i < total_write_times; i++) {
    bool direct_io = true;
    // 到这里我们开始写

    auto ret = sg_write(fd,
                        write_buf,
                        1/*block per write*/,
                        i/*write_to_block_pos*/,
                        k4KiB/*block_size*/,
                        10/*SCSI cmd length*/,
                        false/*FUA*/,
                        false/*dpo*/,
                        &direct_io);
    assert(ret == 0);
    // 写完为了避免threshold.这里加个延时
    Sleep(10);
  }

  auto end_total_time = std::chrono::system_clock::now();

  // 停止计时！

  free(write_buf);
  close(fd);

  return 0;
}