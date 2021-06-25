/*
 *  A utility program for the Linux OS SCSI generic ("sg") device driver.
 *    Copyright (C) 2001 - 2019 D. Gilbert
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later

   This program reads data from the given SCSI device (typically a disk
   or cdrom) and discards that data. Its primary goal is to time
   multiple reads all starting from the same logical address. Its interface
   is a subset of another member of this package: sg_dd which is a
   "dd" variant. The input file can be a scsi generic device, a block device,
   or a seekable file. Streams such as stdin are not acceptable. The block
   size ('bs') is assumed to be 512 if not given.

   This version should compile with Linux sg drivers with version numbers
   >= 30000 . For mmap-ed IO the sg version number >= 30122 .

*/

#define _XOPEN_SOURCE 600
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sg_io_linux.h"
#include "sg_lib.h"
#include "sg_pr2serr.h"
#include "sg_unaligned.h"

static const char *version_str = "1.36 20191220";

#define DEF_BLOCK_SIZE 512
#define DEF_BLOCKS_PER_TRANSFER 128
#define DEF_SCSI_CDBSZ 10
#define MAX_SCSI_CDBSZ 16

#define ME "sg_read: "

#ifndef SG_FLAG_MMAP_IO
#define SG_FLAG_MMAP_IO 4
#endif

#define SENSE_BUFF_LEN 64 /* Arbitrary, could be larger */
#define DEF_TIMEOUT 40000 /* 40,000 millisecs == 40 seconds */

#ifndef RAW_MAJOR
#define RAW_MAJOR 255 /*unlikely value */
#endif

#define FT_OTHER 1  /* filetype other than sg and ... */
#define FT_SG 2     /* filetype is sg char device */
#define FT_RAW 4    /* filetype is raw char device */
#define FT_BLOCK 8  /* filetype is block device */
#define FT_ERROR 64 /* couldn't "stat" file */

#define MIN_RESERVED_SIZE 8192

static int sum_of_resids = 0;

static int64_t total_blocks_to_read = -1;
static int64_t orig_count = 0;
static int64_t in_full = 0;
static int in_partial = 0;

static int recovered_errs = 0;
static int unrecovered_errs = 0;
static int miscompare_errs = 0;

static int pack_id_count = 0;
static int verbose = 0;

static const char *proc_allow_dio = "/proc/scsi/sg/allow_dio";

static void install_handler(int sig_num, void (*sig_handler)(int sig)) {
  struct sigaction sigact;

  sigaction(sig_num, NULL, &sigact);
  if (sigact.sa_handler != SIG_IGN) {
    sigact.sa_handler = sig_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(sig_num, &sigact, NULL);
  }
}

static void print_stats(int iters, const char *str) {
  if (orig_count > 0) {
    if (0 != total_blocks_to_read)
      pr2serr("  remaining block count=%" PRId64 "\n", total_blocks_to_read);
    pr2serr("%" PRId64 "+%d records in", in_full - in_partial, in_partial);
    if (iters > 0)
      pr2serr(", %s commands issued: %d\n", (str ? str : ""), iters);
    else
      pr2serr("\n");
  } else if (iters > 0)
    pr2serr("%s commands issued: %d\n", (str ? str : ""), iters);
}

static void interrupt_handler(int sig) {
  struct sigaction sigact;

  sigact.sa_handler = SIG_DFL;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  sigaction(sig, &sigact, NULL);
  pr2serr("Interrupted by signal,");
  print_stats(0, NULL);
  kill(getpid(), sig);
}

static void siginfo_handler(int sig) {
  if (sig) {
    ;
  } /* unused, dummy to suppress warning */
  pr2serr("Progress report, continuing ...\n");
  print_stats(0, NULL);
}

static int dd_filetype(const char *filename) {
  struct stat st;

  if (stat(filename, &st) < 0)
    return FT_ERROR;
  if (S_ISCHR(st.st_mode)) {
    if (RAW_MAJOR == major(st.st_rdev))
      return FT_RAW;
    else if (SCSI_GENERIC_MAJOR == major(st.st_rdev))
      return FT_SG;
  } else if (S_ISBLK(st.st_mode))
    return FT_BLOCK;
  return FT_OTHER;
}

static void usage() {
  pr2serr("Usage: sg_read  [blk_sgio=0|1] [bpt=BPT] [bs=BS] "
          "[cdbsz=6|10|12|16]\n"
          "                count=COUNT [dio=0|1] [dpo=0|1] [fua=0|1] "
          "if=IFILE\n"
          "                [mmap=0|1] [no_dfxer=0|1] [odir=0|1] "
          "[skip=SKIP]\n"
          "                [time=TI] [verbose=VERB] [--help] "
          "[--verbose]\n"
          "                [--version] "
          "  where:\n"
          "    blk_sgio 0->normal IO for block devices, 1->SCSI commands "
          "via SG_IO\n"
          "    bpt      is blocks_per_transfer (default is 128, or 64 KiB "
          "for default BS)\n"
          "             setting 'bpt=0' will do COUNT zero block SCSI "
          "READs\n"
          "    bs       must match sector size if IFILE accessed via SCSI "
          "commands\n"
          "             (def=512)\n"
          "    cdbsz    size of SCSI READ command (default is 10)\n"
          "    count    total bytes read will be BS*COUNT (if no "
          "error)\n"
          "             (if negative, do |COUNT| zero block SCSI READs)\n"
          "    dio      1-> attempt direct IO on sg device, 0->indirect IO "
          "(def)\n");
  pr2serr("    dpo      1-> set disable page out (DPO) in SCSI READs\n"
          "    fua      1-> set force unit access (FUA) in SCSI READs\n"
          "    if       an sg, block or raw device, or a seekable file (not "
          "stdin)\n"
          "    mmap     1->perform mmaped IO on sg device, 0->indirect IO "
          "(def)\n"
          "    no_dxfer 1->DMA to kernel buffers only, not user space, "
          "0->normal(def)\n"
          "    odir     1->open block device O_DIRECT, 0->don't (def)\n"
          "    skip     each transfer starts at this logical address "
          "(def=0)\n"
          "    time     0->do nothing(def), 1->time from 1st cmd, 2->time "
          "from 2nd, ...\n"
          "    verbose  increase level of verbosity (def: 0)\n"
          "    --help|-h    print this usage message then exit\n"
          "    --verbose|-v   increase level of verbosity (def: 0)\n"
          "    --version|-V   print version number then exit\n\n"
          "Issue SCSI READ commands, each starting from the same logical "
          "block address\n");
}

static int sg_build_scsi_cdb(uint8_t *cdbp, int cdb_sz, unsigned int blocks,
                             int64_t start_block, bool write_true, bool fua,
                             bool dpo) {
  int sz_ind;
  int rd_opcode[] = {0x8, 0x28, 0xa8, 0x88};
  int wr_opcode[] = {0xa, 0x2a, 0xaa, 0x8a};

  memset(cdbp, 0, cdb_sz);
  if (dpo)
    cdbp[1] |= 0x10;
  if (fua)
    cdbp[1] |= 0x8;
  switch (cdb_sz) {
  case 6:
    sz_ind = 0;
    cdbp[0] = (uint8_t)(write_true ? wr_opcode[sz_ind] : rd_opcode[sz_ind]);
    sg_put_unaligned_be24(0x1fffff & start_block, cdbp + 1);
    cdbp[4] = (256 == blocks) ? 0 : (uint8_t)blocks;
    if (blocks > 256) {
      pr2serr(ME "for 6 byte commands, maximum number of blocks is "
                 "256\n");
      return 1;
    }
    if ((start_block + blocks - 1) & (~0x1fffff)) {
      pr2serr(ME "for 6 byte commands, can't address blocks beyond "
                 "%d\n",
              0x1fffff);
      return 1;
    }
    if (dpo || fua) {
      pr2serr(ME "for 6 byte commands, neither dpo nor fua bits "
                 "supported\n");
      return 1;
    }
    break;
  case 10:
    sz_ind = 1;
    cdbp[0] = (uint8_t)(write_true ? wr_opcode[sz_ind] : rd_opcode[sz_ind]);
    sg_put_unaligned_be32((uint32_t)start_block, cdbp + 2);
    sg_put_unaligned_be16((uint16_t)blocks, cdbp + 7);
    if (blocks & (~0xffff)) {
      pr2serr(ME "for 10 byte commands, maximum number of blocks is "
                 "%d\n",
              0xffff);
      return 1;
    }
    break;
  case 12:
    sz_ind = 2;
    cdbp[0] = (uint8_t)(write_true ? wr_opcode[sz_ind] : rd_opcode[sz_ind]);
    sg_put_unaligned_be32((uint32_t)start_block, cdbp + 2);
    sg_put_unaligned_be32((uint32_t)blocks, cdbp + 6);
    break;
  case 16:
    sz_ind = 3;
    cdbp[0] = (uint8_t)(write_true ? wr_opcode[sz_ind] : rd_opcode[sz_ind]);
    sg_put_unaligned_be64(start_block, cdbp + 2);
    sg_put_unaligned_be32((uint32_t)blocks, cdbp + 10);
    break;
  default:
    pr2serr(ME "expected cdb size of 6, 10, 12, or 16 but got %d\n", cdb_sz);
    return 1;
  }
  return 0;
}

/**
 * 函数功能：
 *
 * 发起对设备的读操作
 *
 * 输入参数：
 *    infd:       文件句柄
 *    wrkPos:     内存位置
 *    blocks:     要读的blocks的数目
 *    skip:       从哪里开始读?
 *    bs:         block的大小
 *    scsi_cdbsz: cmd缓冲区的大小
 *    fua:        fua要用吗？
 *    dpo:        device cache要不要被调度出去!
 *    &dio_tmp:   dio_tmp这个值有可能被改变
 *    do_mmap:    是否使用mmap
 *    no_dxfer:   1, 那么就是会走到kernel buffer就结束
 *
 * 返回值:
 *
 *  -3: medium/hardware error,
 *  -2: not ready, 0 -> successful
 *   1: recoverable (ENOMEM)
 *   2: try again (e.g. unit attention),
 *   3: try again (e.g. aborted command)
 *  -1: other unrecoverable error
 */
static int sg_bread(int sg_fd,
                    uint8_t *buff,
                    int blocks,
                    int64_t from_block,
                    int bs,
                    int cdbsz,
                    bool fua,
                    bool dpo,
                    bool *diop,
                    bool do_mmap,
                    bool no_dxfer)
{
  uint8_t rdCmd[MAX_SCSI_CDBSZ];
  uint8_t senseBuff[SENSE_BUFF_LEN];
  struct sg_io_hdr io_hdr;

  printf("from_block = %d\n", from_block);

  // 构建scsi_cdb命令
  if (sg_build_scsi_cdb(rdCmd,
                        cdbsz,
                        blocks,     // 要读多少个blocks
                        from_block, // 从哪个block开始读
                        false,
                        fua,
                        dpo)) {

    pr2serr(ME "bad cdb build, from_block=%" PRId64 ", blocks=%d\n",
            from_block,
            blocks);
    return -1;
  }

  memset(&io_hdr, 0, sizeof(struct sg_io_hdr));

  // interface_id：一般应该设置为 S。 
  io_hdr.interface_id = 'S';

  // cmd_len：指向 SCSI 命令的 cmdp 的字节长度。 
  io_hdr.cmd_len = cdbsz;

  // cmdp：指向将要执行的 SCSI 命令的指针。 
  io_hdr.cmdp = rdCmd;

  if (blocks > 0) {
    // dxfer_direction：用于确定数据传输的方向；常常使用以下值之一： 
    // SG_DXFER_NONE：不需要传输数据。比如 SCSI Test Unit Ready 命令。 
    // SG_DXFER_TO_DEV: 将数据传输到设备。使用 SCSI WRITE 命令。 
    // SG_DXFER_FROM_DEV：从设备输出数据。使用 SCSI READ 命令。 
    // SG_DXFER_TO_FROM_DEV：双向传输数据。 
    // SG_DXFER_UNKNOWN：数据的传输方向未知。 
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;

    // dxfer_len：数据传输的用户内存的长度。 
    io_hdr.dxfer_len = bs * blocks;

    // next: shows dxferp unused during mmap-ed IO
    if (!do_mmap) {
      // dxferp：指向数据传输时长度至少为 dxfer_len
      // 字节的用户内存的指针。 
      io_hdr.dxferp = buff;
    }
    
    if (diop && *diop) {
      io_hdr.flags |= SG_FLAG_DIRECT_IO;
    } else if (do_mmap) {
      io_hdr.flags |= SG_FLAG_MMAP_IO;
    } else if (no_dxfer) {
      io_hdr.flags |= SG_FLAG_NO_DXFER;
    }

  } else {
    io_hdr.dxfer_direction = SG_DXFER_NONE;
  }

  // mx_sb_len：当 sense_buffer 为输出时，可以写回到 sbp 的最大大小。 
  io_hdr.mx_sb_len = SENSE_BUFF_LEN;
  // sbp：缓冲检测指针。 
  io_hdr.sbp = senseBuff;
  // timeout：用于使特定命令超时。 
  io_hdr.timeout = DEF_TIMEOUT;
  // 发送的数据包的序号
  io_hdr.pack_id = pack_id_count++;

  if (verbose > 1) {
    char b[128];

    pr2serr("    READ cdb: %s\n",
            sg_get_command_str(rdCmd, cdbsz, false, sizeof(b), b));
  }

  if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
    if (ENOMEM == errno)
      return 1;
    perror("reading (SG_IO) on sg device, error");
    return -1;
  }

  if (verbose > 2) {
    pr2serr("      duration=%u ms\n", io_hdr.duration);
  }

  // 这里进行处理出错!
  switch (sg_err_category3(&io_hdr)) {
  case SG_LIB_CAT_CLEAN:
    printf("cat_clean\n");
    break;
  case SG_LIB_CAT_RECOVERED:
    if (verbose > 1) {
      sg_chk_n_print3("reading, continue", &io_hdr, true);
    }
    break;
  case SG_LIB_CAT_UNIT_ATTENTION:
    if (verbose) {
      sg_chk_n_print3("reading", &io_hdr, (verbose > 1));
    }
    return 2;
  case SG_LIB_CAT_ABORTED_COMMAND:
    if (verbose) {
      sg_chk_n_print3("reading", &io_hdr, (verbose > 1));
    }
    return 3;
  case SG_LIB_CAT_NOT_READY:
    if (verbose) {
      sg_chk_n_print3("reading", &io_hdr, (verbose > 1));
    }
    return -2;
  case SG_LIB_CAT_MEDIUM_HARD:
    if (verbose) {
      sg_chk_n_print3("reading", &io_hdr, (verbose > 1));
    }
    return -3;
  default:
    sg_chk_n_print3("reading", &io_hdr, !!verbose);
    return -1;
  }

  if (blocks > 0) {
    if (diop && *diop &&
        ((io_hdr.info & SG_INFO_DIRECT_IO_MASK) != SG_INFO_DIRECT_IO)) {
      *diop = 0; /* flag that dio not done (completely) */
      printf("diop not done (completely)\n");
    }
    sum_of_resids += io_hdr.resid;
  }

  return 0;
}

/* Returns the number of times 'ch' is found in string 's' given the
 * string's length. */
static int num_chs_in_str(const char *s, int slen, int ch) {
  int res = 0;

  while (--slen >= 0) {
    if (ch == s[slen])
      ++res;
  }
  return res;
}


static bool do_verify = false;

#define VERIFY10 0x2f
#define VERIFY12 0xaf
#define VERIFY16 0x8f

static int
sg_build_scsi_cdb_write(uint8_t * cdbp,
                        int cdb_sz,
                        unsigned int blocks,
                        int64_t start_block,
                        bool is_verify,
                        bool write_true,
                        bool fua,
                        bool dpo,
                        int cdl)
{
    int sz_ind;
    int rd_opcode[] = {0x8, 0x28, 0xa8, 0x88};
    int ve_opcode[] = {0xff /* no VERIFY(6) */, VERIFY10, VERIFY12, VERIFY16};
    int wr_opcode[] = {0xa, 0x2a, 0xaa, 0x8a};

    memset(cdbp, 0, cdb_sz);

    if (is_verify) {
        cdbp[1] = 0x2;  /* (BYTCHK=1) << 1 */
    } else {
        if (dpo) {
            cdbp[1] |= 0x10;
        }

        if (fua) {
            cdbp[1] |= 0x8;
        }
    }

    switch (cdb_sz) {
    case 6:
        sz_ind = 0;
        if (is_verify && write_true) {
            pr2serr(ME "there is no VERIFY(6), choose a larger cdbsz\n");
            return 1;
        }
        cdbp[0] = (uint8_t)(write_true ? wr_opcode[sz_ind] :
                                               rd_opcode[sz_ind]);
        sg_put_unaligned_be24(0x1fffff & start_block, cdbp + 1);
        cdbp[4] = (256 == blocks) ? 0 : (uint8_t)blocks;
        if (blocks > 256) {
            pr2serr(ME "for 6 byte commands, maximum number of blocks is "
                    "256\n");
            return 1;
        }
        if ((start_block + blocks - 1) & (~0x1fffff)) {
            pr2serr(ME "for 6 byte commands, can't address blocks beyond "
                    "%d\n", 0x1fffff);
            return 1;
        }
        if (dpo || fua) {
            pr2serr(ME "for 6 byte commands, neither dpo nor fua bits "
                    "supported\n");
            return 1;
        }
        break;
    case 10:
        sz_ind = 1;
        if (is_verify && write_true)
            cdbp[0] = ve_opcode[sz_ind];
        else
            cdbp[0] = (uint8_t)(write_true ? wr_opcode[sz_ind] :
                                             rd_opcode[sz_ind]);
        sg_put_unaligned_be32(start_block, cdbp + 2);
        sg_put_unaligned_be16(blocks, cdbp + 7);
        if (blocks & (~0xffff)) {
            pr2serr(ME "for 10 byte commands, maximum number of blocks is "
                    "%d\n", 0xffff);
            return 1;
        }
        break;
    case 12:
        sz_ind = 2;
        if (is_verify && write_true)
            cdbp[0] = ve_opcode[sz_ind];
        else
            cdbp[0] = (uint8_t)(write_true ? wr_opcode[sz_ind] :
                                             rd_opcode[sz_ind]);
        sg_put_unaligned_be32(start_block, cdbp + 2);
        sg_put_unaligned_be32(blocks, cdbp + 6);
        break;
    case 16:
        sz_ind = 3;
        if (is_verify && write_true)
            cdbp[0] = ve_opcode[sz_ind];
        else
            cdbp[0] = (uint8_t)(write_true ? wr_opcode[sz_ind] :
                                             rd_opcode[sz_ind]);
        if ((! is_verify) && (cdl > 0)) {
            if (cdl & 0x4)
                cdbp[1] |= 0x1;
            if (cdl & 0x3)
                cdbp[14] |= ((cdl & 0x3) << 6);
        }
        sg_put_unaligned_be64(start_block, cdbp + 2);
        sg_put_unaligned_be32(blocks, cdbp + 10);
        break;
    default:
        pr2serr(ME "expected cdb size of 6, 10, 12, or 16 but got %d\n",
                cdb_sz);
        return 1;
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

/**
 *
 * 函数功能：
 *
 *  调用SCSI命令写一个设备!
 *  Does a SCSI WRITE or VERIFY (if do_verify set) on OFILE.
 *
 * 输入参数：
 *
 *  - sg_fd:      需要写的sg设备
 *  - buff:       写的内容所在缓冲区
 *  - blocks:     要写的blocks的数目
 *  - to_block:   写到磁盘上的位置, 这个是以block为单位
 *  - bs:         block size, 单个 block的大小，必须是物理block的整数倍
 *  - ofp:        写的时候的一些flag
 *  - diop:       是direct io吗？
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
         uint8_t * buff,
         int blocks,
         int64_t to_block,
         int bs,
         int cdbsz,
         bool fua,
         bool dpo,
         bool *diop,
         bool do_mmap,
         bool no_dxfer)
{
    bool info_valid;
    int res;
    uint64_t io_addr = 0;
    const char * op_str = do_verify ? "verifying" : "writing";
    uint8_t wrCmd[MAX_SCSI_CDBSZ];
    uint8_t senseBuff[SENSE_BUFF_LEN];
    struct sg_io_hdr io_hdr;

    if (sg_build_scsi_cdb_write(wrCmd,
                                cdbsz,
                                blocks,
                                to_block,
                                do_verify,
                                true,
                                fua,
                                dpo,
                                0/*cdl*/)) {
        pr2serr(ME "bad wr cdb build, to_block=%" PRId64 ", blocks=%d\n",
                to_block, blocks);
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

    if (verbose > 2) {
        sg_print_command_len(wrCmd, cdbsz);
    }

    while (((res = ioctl(sg_fd, SG_IO, &io_hdr)) < 0) &&
           ((EINTR == errno) || (EAGAIN == errno) || (EBUSY == errno)))
        ;
    if (res < 0) {
        if (ENOMEM == errno)
            return -2;
        if (do_verify)
            perror("verifying (SG_IO) on sg device, error");
        else
            perror("writing (SG_IO) on sg device, error");
        return -1;
    }

    if (verbose > 2)
        pr2serr("      duration=%u ms\n", io_hdr.duration);
    res = sg_err_category3(&io_hdr);
    switch (res) {
    case SG_LIB_CAT_CLEAN:
    case SG_LIB_CAT_CONDITION_MET:
        break;
    case SG_LIB_CAT_RECOVERED:
        ++recovered_errs;
        info_valid = sg_get_sense_info_fld(io_hdr.sbp, io_hdr.sb_len_wr,
                                           &io_addr);
        if (info_valid) {
            pr2serr("    lba of last recovered error in this WRITE=0x%" PRIx64
                    "\n", io_addr);
            if (verbose > 1)
                sg_chk_n_print3(op_str, &io_hdr, true);
        } else {
            pr2serr("Recovered error: [no info] %s to block=0x%" PRIx64
                    ", num=%d\n", op_str, to_block, blocks);
            sg_chk_n_print3(op_str, &io_hdr, verbose > 1);
        }
        break;
    case SG_LIB_CAT_ABORTED_COMMAND:
    case SG_LIB_CAT_UNIT_ATTENTION:
        sg_chk_n_print3(op_str, &io_hdr, verbose > 1);
        return res;
    case SG_LIB_CAT_MISCOMPARE: /* must be VERIFY cpommand */
        ++miscompare_errs;
        if (coe) {
            if (verbose > 1)
                pr2serr(">> bypass due to miscompare: out blk=%" PRId64
                        " for %d blocks\n", to_block, blocks);
            return SG_DD_BYPASS; /* fudge success */
        } else {
            pr2serr("VERIFY reports miscompare\n");
            return res;
        }
    case SG_LIB_CAT_NOT_READY:
        ++unrecovered_errs;
        pr2serr("device not ready (w)\n");
        return res;
    case SG_LIB_CAT_MEDIUM_HARD:
    default:
        sg_chk_n_print3(op_str, &io_hdr, verbose > 1);
        if ((SG_LIB_CAT_ILLEGAL_REQ == res) && verbose)
            sg_print_command_len(wrCmd, cdbsz);
        ++unrecovered_errs;
        if (coe) {
            if (verbose > 1)
                pr2serr(">> ignored errors for out blk=%" PRId64 " for %d "
                        "bytes\n", to_block, bs * blocks);
            return SG_DD_BYPASS; /* fudge success */
        } else
            return res;
    }
    if (diop && *diop &&
        ((io_hdr.info & SG_INFO_DIRECT_IO_MASK) != SG_INFO_DIRECT_IO))
        *diop = false;      /* flag that dio not done (completely) */
    return 0;
}

#define STR_SZ 1024
#define INF_SZ 512
#define EBUFF_SZ 768

int main(int argc, char *argv[]) {
  // 要读的blocks的数量
  // when COUNT is a positive number, read that number of blocks, typically with
  // multiple read operations. When COUNT is negative then |COUNT| SCSI READ
  // commands are performed requesting zero blocks to be transferred. This
  // option is mandatory.
  bool has_set_count = false;

  bool dio_tmp;

  // blk_sgio:
  //
  // The default action of this utility is to use the Unix read()
  // command when the
  // IFILE is a block device. In lk 2.6 many block devices can
  // handle SCSI commands
  // issued via the SG_IO ioctl. So when this option is set the
  // SG_IO ioctl sends
  // SCSI READ commands to IFILE if it is a block device.
  //
  // 1. (缺省)情况下是使用 unix-read函数
  // 2. 然后从linux-2.6开始，设备可以处理SCSI的命令
  // 所以如果开启了这个选项，那么就可以使用IOCTL SG_IO命令来发送
  // SCSI READ命令
  //
  // The default action of this utility is to use the Unix read()
  // command when the
  // IFILE is a block device. In lk 2.6 many block devices can handle
  // SCSI commands
  // issued via the SG_IO ioctl. So when this option is set the SG_IO ioctl
  // sends SCSI READ commands to IFILE if it is a block device.

  bool do_blk_sgio = false;

  // do_dio:
  // 1: 尝试在sg device上直接进行direct io
  // 0: indirect IO (def)

  // default is 0 which selects indirect IO. Value of 1 attempts direct IO
  // which, if not available, falls back to indirect IO and notes this at
  // completion. This option is only active if IFILE is an sg device. If direct
  // IO is selected and /proc/scsi/sg/allow_dio has the value of 0 then a
  // warning is issued (and indirect IO is performed)
  bool do_dio = false;

  // 1: 在SG设备上进行mapp IO
  // 0: indirect IO (def)
  // default is 0 which selects indirect IO.
  // Value of 1 causes memory mapped IO to be
  // performed. Selecting both dio and mmap is an error.
  // This option is only active if IFILE is an sg device.
  bool do_mmap = false;

  // 1: 采用O_DIRECT来打开设备
  // 0: don't (def)
  // when set opens an IFILE which is a block device with
  // an additional O_DIRECT
  // flag. The default value is 0
  // (i.e. don't open block devices O_DIRECT).
  bool do_odir = false;

  // 1: set disable page out (DPO) in SCSI READs
  //
  // disable page out changes the cache retention
  // priority of blocks read on the device's cache
  // to the lowest priority.
  //
  // This means that blocks
  // read by other commands are more likely to
  // remain in the device's cache.
  // 这里的意是说，想让device's里面的cache尽量不要被调出去!
  bool dpo = false;

  // 1: set force unit access (FUA) in SCSI READs
  // FUA命令可以跳过 硬盘的缓存，直接将数据写入到硬盘中，
  // 这个不用管!
  // when set the force unit access (FUA) bit in SCSI
  // READ commands is set. Otherwise
  // the FUA bit is cleared (default).
  bool fua = false;

  // no_dxfer:
  //
  // 1: when set then DMA transfers from the device are made
  // into kernel buffers but no
  // further (i.e. there is no second copy into the user
  // space).
  // 0: (Default) in which case transfers are made into the user space.
  //
  // When neither mmap nor
  // dio is set then data transfer are copied via kernel
  // buffers (i.e. a double
  // copy). Mainly for testing.
  //
  // 当mmap和dio都没有设置的时候，都会走kernel buffer，产生两次copy
  bool no_dxfer = false;

  // BS就是每次eache block的大小，必须要是物理block size的整数倍
  // 需要是bytes
  //
  // where BS is the size (in bytes) of each block read.
  // This must be the block size
  // of the physical device (defaults to 512) if SCSI
  // commands are being issued to
  // IFILE.
  // 相当于我认为的block size
  // block size in bytes
  int bs = 0;

  // 每次read操作: 能够取的最大的blocks的数目(128=default)
  //
  // where BPT is the maximum number of blocks each read operation fetches.
  // Fewer blocks will be fetched when the remaining COUNT is less than BPT.
  //
  // The default value for BPT is 128. Note that each read operation starts
  // at the same lba (as given by skip=SKIP or 0).
  //
  // If 'bpt=0' then the COUNT is interpreted as the number
  // of zero block SCSI READ commands to issue.
  //
  int bpt = DEF_BLOCKS_PER_TRANSFER;

  int dio_incomplete = 0;

  //
  // 0: (default) doesn't perform timing.
  //
  // 1: times transfer and does throughput calculation
  //  starting at the first issued command until completion.
  //
  // 2: times transfer and does throughput calculation,
  //  starting at the second issued command until completion.
  // 3: times from third command, etc. An average number of
  //  commands (SCSI READs or Unix read()s)
  //
  // output: executed per second is also output.
  //
  int do_time = 0;

  int in_type = FT_OTHER;
  int ret = 0;

  // SCSI cbd命令长度
  // size of SCSI READ commands issued on sg device names,
  // or block devices if
  // 'blk_sgio=1' is given. Default is 10 byte SCSI READ cdbs.

  int scsi_cdbsz = DEF_SCSI_CDBSZ;

  // all read operations will start offset by
  // SKIP bs-sized blocks from the start of
  // the input file (or device).
  int64_t skip = 0;

  int res;
  int k;
  int buf_sz;
  int iters;
  int infd;
  int blocks;
  int flags;
  int blocks_per;
  int err;
  int n;
  int keylen;

  bool verbose_given = false;
  bool version_given = false;

  size_t psz;

  char *key;
  char *buf;
  uint8_t *wrkBuff = NULL;
  uint8_t *wrkPos = NULL;
  char inf[INF_SZ];
  char outf[INF_SZ];
  char str[STR_SZ];
  char ebuff[EBUFF_SZ];
  const char *read_str;
  struct timeval start_tm, end_tm;

  // psz
#if defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE)
  psz = sysconf(_SC_PAGESIZE); /* POSIX.1 (was getpagesize()) */
#else
  psz = 4096; /* give up, pick likely figure */
#endif
  inf[0] = '\0';

  for (k = 1; k < argc; k++) {
    if (argv[k]) {
      strncpy(str, argv[k], STR_SZ);
      str[STR_SZ - 1] = '\0';
    } else
      continue;

    for (key = str, buf = key; (*buf && (*buf != '='));)
      buf++;

    if (*buf)
      *buf++ = '\0';

    keylen = strlen(key);

    // blk_sgio:
    // - 0: normal IO for block devices,
    // - 1: SCSI commands via SG_IO
    if (0 == strcmp(key, "blk_sgio"))
      do_blk_sgio = !!sg_get_num(buf);

    // bpt : blocks_per_transfer
    //       (default is 128, or
    //        64 KiB for default BS)
    //       setting 'bpt=0' will do
    //       COUNT zero block SCSI READs
    else if (0 == strcmp(key, "bpt")) {
      bpt = sg_get_num(buf);
      if (-1 == bpt) {
        pr2serr(ME "bad argument to 'bpt'\n");
        return SG_LIB_SYNTAX_ERROR;
      }

      // bs: must match sector size if IFILE
      //      accessed via SCSI commands
      //      (def=512)
      // bs: 如果是采用SCSI命令来访问IFILE的话，那么
      // bs需要匹配SCSI命令里面的sector size.
      // 缺省是512.
    } else if (0 == strcmp(key, "bs")) {
      bs = sg_get_num(buf);
      if (-1 == bs) {
        pr2serr(ME "bad argument to 'bs'\n");
        return SG_LIB_SYNTAX_ERROR;
      }

      // cdbsz: size of SCSI READ command (default is 10)
      // SCSI READ命令的size. 缺省是10.
    } else if (0 == strcmp(key, "cdbsz"))
      scsi_cdbsz = sg_get_num(buf);

    // count: total bytes read will be BS*COUNT (if no error)
    // 总共会读的bytes = BS * COUNT
    else if (0 == strcmp(key, "count")) {
      has_set_count = true;
      if ('-' == *buf) {
        total_blocks_to_read = sg_get_llnum(buf + 1);
        if (-1 == total_blocks_to_read) {
          pr2serr(ME "bad argument to 'count'\n");
          return SG_LIB_SYNTAX_ERROR;
        }
        total_blocks_to_read = -total_blocks_to_read;
      } else {
        total_blocks_to_read = sg_get_llnum(buf);
        if (-1 == total_blocks_to_read) {
          pr2serr(ME "bad argument to 'count'\n");
          return SG_LIB_SYNTAX_ERROR;
        }
      }

      // dio:
      // 1-> attempt direct IO on sg device
      // 0-> indirect IO (def)
    } else if (0 == strcmp(key, "dio"))
      do_dio = !!sg_get_num(buf);

    // dpo:
    // 1: set disable page out (DPO) in SCSI READs
    else if (0 == strcmp(key, "dpo"))
      dpo = !!sg_get_num(buf);

    // fua:
    // 1-> set force unit access (FUA) in SCSI READs
    else if (0 == strcmp(key, "fua"))
      fua = !!sg_get_num(buf);

    // if: an sg, block or raw device,
    //     or a seekable file (not stdin)
    else if (strcmp(key, "if") == 0) {
      // 就是拷贝我们的设备文件的名字
      memcpy(inf, buf, INF_SZ - 1);
      inf[INF_SZ - 1] = '\0';

      // mmap:
      // 1->perform mmaped IO on sg device
      // 0->indirect IO (def)
    } else if (0 == strcmp(key, "mmap"))
      do_mmap = !!sg_get_num(buf);

    // no_dxfer:
    // 1: DMA to kernel buffers only, not user space
    // 0: normal(def)
    else if (0 == strcmp(key, "no_dxfer"))
      no_dxfer = !!sg_get_num(buf);

    // do_odir:
    // 1: open block device O_DIRECT,
    // 0: don't (def)
    else if (0 == strcmp(key, "odir"))
      do_odir = !!sg_get_num(buf);

    // of:
    else if (strcmp(key, "of") == 0) {
      memcpy(outf, buf, INF_SZ - 1);
      outf[INF_SZ - 1] = '\0';

      // skip:
      // each transfer starts at this logical address (def=0)
    } else if (0 == strcmp(key, "skip")) {
      skip = sg_get_llnum(buf);
      if (-1 == skip) {
        pr2serr(ME "bad argument to 'skip'\n");
        return SG_LIB_SYNTAX_ERROR;
      }

    } else if (0 == strcmp(key, "time"))
      do_time = sg_get_num(buf);

    else if (0 == strncmp(key, "verb", 4)) {
      verbose_given = true;
      verbose = sg_get_num(buf);

    } else if (0 == strncmp(key, "--help", 6)) {
      usage();
      return 0;

    } else if (0 == strncmp(key, "--verb", 6)) {
      verbose_given = true;
      ++verbose;

    } else if (0 == strncmp(key, "--vers", 6))
      version_given = true;

    else if ((keylen > 1) && ('-' == key[0]) && ('-' != key[1])) {
      res = 0;
      n = num_chs_in_str(key + 1, keylen - 1, 'h');
      if (n > 0) {
        usage();
        return 0;
      }

      n = num_chs_in_str(key + 1, keylen - 1, 'v');
      if (n > 0)
        verbose_given = true;

      verbose += n;
      res += n;
      n = num_chs_in_str(key + 1, keylen - 1, 'V');
      if (n > 0)
        version_given = true;

      res += n;
      if (res < (keylen - 1)) {
        pr2serr("Unrecognised short option in '%s', try '--help'\n", key);
        return SG_LIB_SYNTAX_ERROR;
      }
    } else {
      pr2serr("Unrecognized argument '%s'\n", key);
      usage();
      return SG_LIB_SYNTAX_ERROR;
    }
  }

#ifdef DEBUG
  pr2serr("In DEBUG mode, ");
  if (verbose_given && version_given) {
    pr2serr("but override: '-vV' given, zero verbose and continue\n");
    verbose_given = false;
    version_given = false;
    verbose = 0;
  } else if (!verbose_given) {
    pr2serr("set '-vv'\n");
    verbose = 2;
  } else
    pr2serr("keep verbose=%d\n", verbose);
#else
  if (verbose_given && version_given)
    pr2serr("Not in DEBUG mode, so '-vV' has no special action\n");
#endif

  if (version_given) {
    pr2serr(ME ": %s\n", version_str);
    return 0;
  }

  // 正式开始准备读写
  // block_size
  if (bs <= 0) {
    bs = DEF_BLOCK_SIZE;
    if ((total_blocks_to_read > 0) && (bpt > 0))
      pr2serr("Assume default 'bs' (block size) of %d bytes\n", bs);
  }

  // 总共需要读的block数目
  // block / bpt = 发送的read cmd的次数
  if (!has_set_count) {
    pr2serr("'count' must be given\n");
    usage();
    return SG_LIB_SYNTAX_ERROR;
  }

  if (skip < 0) {
    pr2serr("skip cannot be negative\n");
    return SG_LIB_SYNTAX_ERROR;
  }

  if (bpt < 1) {
    if (0 == bpt) {
      if (total_blocks_to_read > 0)
        total_blocks_to_read = -total_blocks_to_read;
    } else {
      pr2serr("bpt must be greater than 0\n");
      return SG_LIB_SYNTAX_ERROR;
    }
  }

  if (do_dio && do_mmap) {
    pr2serr("cannot select both dio and mmap\n");
    return SG_LIB_CONTRADICT;
  }

  if (no_dxfer && (do_dio || do_mmap)) {
    pr2serr("cannot select no_dxfer with dio or mmap\n");
    return SG_LIB_CONTRADICT;
  }

  install_handler(SIGINT, interrupt_handler);
  install_handler(SIGQUIT, interrupt_handler);
  install_handler(SIGPIPE, interrupt_handler);
  install_handler(SIGUSR1, siginfo_handler);

  if (!inf[0]) {
    pr2serr("must provide 'if=<filename>'\n");
    usage();
    return SG_LIB_SYNTAX_ERROR;
  }

  if (0 == strcmp("-", inf)) {
    pr2serr("'-' (stdin) invalid as <filename>\n");
    usage();
    return SG_LIB_SYNTAX_ERROR;
  }

  in_type = dd_filetype(inf);
  if (FT_ERROR == in_type) {
    pr2serr("Unable to access: %s\n", inf);
    return SG_LIB_FILE_ERROR;
  } else if ((FT_BLOCK & in_type) && do_blk_sgio)
    // 这里采用了FT_SG
    in_type |= FT_SG;
  
  if (in_type & FT_SG) {
    printf("FT_SG has set!\n");
  }

  // ./sg_read blk_sgio=1 bs=4096  bpt=1 count=1 
  //               if=/dev/sdb odir=1 verbose=10
  // FT_SG has set!
  // Opened /dev/sdb for SG_IO with flags=0x4002
  //  READ cdb: [28 00 00 00 00 00 00 00 01 00]
  //    duration=0 ms
  // 1+0 records in, SCSI READ commands issued: 1


  if (FT_SG & in_type) {
    if ((total_blocks_to_read < 0) && (6 == scsi_cdbsz)) {
      pr2serr(ME "SCSI READ (6) can't do zero block reads\n");
      return SG_LIB_SYNTAX_ERROR;
    }

    // 设置flag
    flags = O_RDWR;
    if (do_odir)
      flags |= O_DIRECT;

    // 打开设备
    if ((infd = open(inf, flags)) < 0) {
      flags = O_RDONLY;
      if (do_odir)
        flags |= O_DIRECT;
      if ((infd = open(inf, flags)) < 0) {
        err = errno;
        snprintf(ebuff, EBUFF_SZ,
          ME "could not open %s for sg reading", inf);
        perror(ebuff);
        return sg_convert_errno(err);
      }
    }

    if (verbose) {
      pr2serr("Opened %s for SG_IO with flags=0x%x\n", inf, flags);
    }

    // 不会跳到这里来，先不用看
    // total_blocks_to_read = 1
    if ((total_blocks_to_read > 0) && (!(FT_BLOCK & in_type))) {
      assert(0);
      // deleted code
    }
  } else {
    assert(0);
    // deleted code
  }

  assert(total_blocks_to_read > 0);

  orig_count = total_blocks_to_read;

  if (total_blocks_to_read > 0) {
    if (do_dio || do_odir || (FT_RAW & in_type)) {
      // 必须要走这里过!
      // 申请好内存!
      assert(0 == posix_memalign(&wrkBuff, 4096, bs * bpt + psz));
      assert(wrkBuff);
      wrkPos = wrkBuff;
    } else if (do_mmap) {
      assert(0);
      // 如果走mmap!
      wrkPos = (uint8_t *)mmap(NULL, bs * bpt, PROT_READ | PROT_WRITE,
                               MAP_SHARED, infd, 0);
      if (MAP_FAILED == wrkPos) {
        perror(ME "error from mmap()");
        return SG_LIB_CAT_OTHER;
      }
    } else {
      assert(0);
      // 如果malloc!
      wrkBuff = (uint8_t *)malloc(bs * bpt);
      if (0 == wrkBuff) {
        pr2serr("Not enough user memory\n");
        return SG_LIB_CAT_OTHER;
      }
      wrkPos = wrkBuff;
    }
  }

  blocks_per = bpt;
  start_tm.tv_sec = 0; /* just in case start set condition not met */
  start_tm.tv_usec = 0;

  if (verbose && (total_blocks_to_read < 0))
    pr2serr("About to issue %" PRId64
      " zero block SCSI READs\n", 0 - total_blocks_to_read);

  /* main loop */
  for (iters = 0; total_blocks_to_read != 0; ++iters) {
    // do_time的含义就是从第几个IO request开始计时
    // 如果time=1，那么就是从第一个IO开始计时
    // 之所以这样设计是有的设备是有预热时间
    if ((do_time > 0) && (iters == (do_time - 1))) {
      gettimeofday(&start_tm, NULL);
    }

    // 这里是设置每次要读的block的数目
    // 如果total_blocks_to_read是一个负数
    // 那么这里要读的blocks = 0
    if (total_blocks_to_read < 0) {
      blocks = 0;
    } else {
      blocks = (total_blocks_to_read > blocks_per) ?
        blocks_per : total_blocks_to_read;
    }

    assert(FT_SG & in_type);

    if (FT_SG & in_type) {
      dio_tmp = do_dio;

      res = sg_bread(infd,        // 文件句柄
                     wrkPos,      // 内存位置
                     blocks,      // 要读的blocks的数目
                     skip,        // 从哪里开始读?
                     bs,          // block的大小
                     scsi_cdbsz,  // cmd缓冲区的大小
                     fua,         // fua要用吗？
                     dpo,         // device cache要不要被调度出去!
                     &dio_tmp,    // dio_tmp这个值有可能被改变
                     do_mmap,     // 是否使用mmap
                     no_dxfer);   // =1, 那么就是会走到kernel buffer就结束

      // 出错1
      // ERROR: ENOMEM,
      // find what's available+try that
      if (1 == res) {
        if (ioctl(infd, SG_GET_RESERVED_SIZE, &buf_sz) < 0) {
          perror("RESERVED_SIZE ioctls failed");
          break;
        }

        if (buf_sz < MIN_RESERVED_SIZE) {
          buf_sz = MIN_RESERVED_SIZE;
        }

        // 重新调整了要读的blocks数目
        blocks_per = (buf_sz + bs - 1) / bs;
        blocks = blocks_per;

        pr2serr("Reducing read to %d blocks per loop\n", blocks_per);

        // 再次去trigger read
        res = sg_bread(infd,
                       wrkPos,
                       blocks,
                       skip,
                       bs,
                       scsi_cdbsz,
                       fua,
                       dpo,
                       &dio_tmp,
                       do_mmap,
                       no_dxfer);

      } else if (2 == res) {
        // 出错2
        // ERROR: 直接要求重试
        pr2serr("Unit attention, try again (r)\n");
        res = sg_bread(infd, wrkPos, blocks, skip, bs, scsi_cdbsz, fua, dpo,
                       &dio_tmp, do_mmap, no_dxfer);
      }

      // 出错3
      if (0 != res) {
        switch (res) {
        case -3:
          ret = SG_LIB_CAT_MEDIUM_HARD;
          pr2serr(ME "SCSI READ medium/hardware error\n");
          break;
        case -2:
          ret = SG_LIB_CAT_NOT_READY;
          pr2serr(ME "device not ready\n");
          break;
        case 2:
          ret = SG_LIB_CAT_UNIT_ATTENTION;
          pr2serr(ME "SCSI READ unit attention\n");
          break;
        case 3:
          ret = SG_LIB_CAT_ABORTED_COMMAND;
          pr2serr(ME "SCSI READ aborted command\n");
          break;
        default:
          ret = SG_LIB_CAT_OTHER;
          pr2serr(ME "SCSI READ failed\n");
          break;
        }
        break;
      } else {

        // 如果成功!
        in_full += blocks;
        if (do_dio && (0 == dio_tmp)) {
          // 如果是DIO，然后还没有做完!
          dio_incomplete++;
        }
      }
    } else {
      assert(0);
      // deleted some code.
    }

    // 调整total_blocks_to_read
    if (total_blocks_to_read > 0) {
      total_blocks_to_read -= blocks;
    } else if (total_blocks_to_read < 0) {
      ++total_blocks_to_read;
    }
  } // ! main loop 读循环结束

  read_str = (FT_SG & in_type) ? "SCSI READ" : "read";

  // 这里开始计算结束时间!
  // 这一堆代码不用管
  if (do_time > 0) {
    gettimeofday(&end_tm, NULL);
    if (start_tm.tv_sec || start_tm.tv_usec) {
      struct timeval res_tm;
      double a, b, c;

      res_tm.tv_sec = end_tm.tv_sec - start_tm.tv_sec;
      res_tm.tv_usec = end_tm.tv_usec - start_tm.tv_usec;
      if (res_tm.tv_usec < 0) {
        --res_tm.tv_sec;
        res_tm.tv_usec += 1000000;
      }
      a = res_tm.tv_sec;
      a += (0.000001 * res_tm.tv_usec);
      if (orig_count > 0) {
        b = (double)bs * (orig_count - total_blocks_to_read);
        if (do_time > 1)
          c = b - ((double)bs * ((do_time - 1.0) * bpt));
        else
          c = 0.0;
      } else {
        b = 0.0;
        c = 0.0;
      }

      if (1 == do_time) {
        pr2serr("Time for all %s commands was %d.%06d secs", read_str,
                (int)res_tm.tv_sec, (int)res_tm.tv_usec);
        if ((orig_count > 0) && (a > 0.00001) && (b > 511))
          pr2serr(", %.2f MB/sec\n", b / (a * 1000000.0));
        else
          pr2serr("\n");
      } else if (2 == do_time) {
        pr2serr("Time from second %s command to end was %d.%06d secs", read_str,
                (int)res_tm.tv_sec, (int)res_tm.tv_usec);
        if ((orig_count > 0) && (a > 0.00001) && (c > 511))
          pr2serr(", %.2f MB/sec\n", c / (a * 1000000.0));
        else
          pr2serr("\n");
      } else {
        pr2serr("Time from start of %s command "
                "#%d to end was %d.%06d secs",
                read_str, do_time, (int)res_tm.tv_sec, (int)res_tm.tv_usec);
        if ((orig_count > 0) && (a > 0.00001) && (c > 511))
          pr2serr(", %.2f MB/sec\n", c / (a * 1000000.0));
        else
          pr2serr("\n");
      }
      if ((iters > 0) && (a > 0.00001))
        pr2serr("Average number of %s commands per second was %.2f\n",
                read_str,
                (double)iters / a);
    }
  }

  // 释放内存
  if (wrkBuff) {
    free(wrkBuff);
  }

  // 关闭文件
  close(infd);

  // 如果还没有读完，那么就是有出错!
  if (0 != total_blocks_to_read) {
    pr2serr("Some error occurred,");
    if (0 == ret)
      ret = SG_LIB_CAT_OTHER;
  }
  print_stats(iters, read_str);

  if (dio_incomplete) {
    int fd;
    char c;

    pr2serr(">> Direct IO requested but incomplete %d times\n", dio_incomplete);
    if ((fd = open(proc_allow_dio, O_RDONLY)) >= 0) {
      if (1 == read(fd, &c, 1)) {
        if ('0' == c)
          pr2serr(">>> %s set to '0' but should be set to '1' for "
                  "direct IO\n",
                  proc_allow_dio);
      }
      close(fd);
    }
  }

  if (sum_of_resids) {
    pr2serr(">> Non-zero sum of residual counts=%d\n", sum_of_resids);
  }

  SG_LIB_SYNTAX_ERROR;
  // main函数的返回值
  return (ret >= 0) ? ret : SG_LIB_CAT_OTHER;
}
