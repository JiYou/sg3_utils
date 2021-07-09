/*
 * A utility program for the Linux OS SCSI generic ("sg") device driver.
 * Copyright (C) 1999-2010 D. Gilbert and P. Allworth
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is a specialization of the Unix "dd" command in which
 * one or both of the given files is a scsi generic device or a raw
 * device. A block size ('bs') is assumed to be 512 if not given. This
 * program complains if 'ibs' or 'obs' are given with some other value
 * than 'bs'. If 'if' is not given or 'if=-' then stdin is assumed. If
 * 'of' is not given or 'of=-' then stdout assumed.  Multipliers:
 *    'c','C'  *1       'b','B' *512      'k' *1024      'K' *1000
 *    'm' *(1024^2)     'M' *(1000^2)     'g' *(1024^3)  'G' *(1000^3)
 *
 * A non-standard argument "bpt" (blocks per transfer) is added to control
 * the maximum number of blocks in each transfer. The default value is 128.
 * For example if "bs=512" and "bpt=32" then a maximum of 32 blocks (16KB
 * in this case) are transferred to or from the sg device in a single SCSI
 * command.
 *
 * This version should compile with Linux sg drivers with version numbers
 * >= 30000 . This version uses queuing within the Linux sg driver.
 */

/**
 * root@yoj-perf-4g:/opt/sg3_utils/examples# strace ./sgq_dd if=/dev/sg0
 *of=/dev/sg2 bs=4096 bpt=1 count=1 execve("./sgq_dd", ["./sgq_dd",
 *"if=/dev/sg0", "of=/dev/sg2", "bs=4096", "bpt=1", "count=1"], 0x7ffed80a92a8
 *24 vars ) = 0 brk(NULL)                               = 0x55e0ba246000
 * access("/etc/ld.so.nohwcap", F_OK)      = -1 ENOENT (No such file or
 *directory) access("/etc/ld.so.preload", R_OK)      = -1 ENOENT (No such file
 *or directory) openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
 * fstat(3, {st_mode=S_IFREG|0644, st_size=31819, ...}) = 0
 * mmap(NULL, 31819, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7fb1f6277000
 * close(3)                                = 0
 * access("/etc/ld.so.nohwcap", F_OK)      = -1 ENOENT (No such file or
 *directory) openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6",
 *O_RDONLY|O_CLOEXEC) = 3 read(3,
 *"\177ELF\2\1\1\3\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\20\35\2\0\0\0\0\0"..., 832) =
 *832 fstat(3, {st_mode=S_IFREG|0755, st_size=2030928, ...}) = 0 mmap(NULL,
 *8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7fb1f6275000
 * mmap(NULL, 4131552, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) =
 *0x7fb1f5c65000 mprotect(0x7fb1f5e4c000, 2097152, PROT_NONE) = 0
 * mmap(0x7fb1f604c000, 24576, PROT_READ|PROT_WRITE,
 *MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1e7000) = 0x7fb1f604c000
 * mmap(0x7fb1f6052000, 15072, PROT_READ|PROT_WRITE,
 *MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x7fb1f6052000 close(3) = 0
 * arch_prctl(ARCH_SET_FS, 0x7fb1f6276540) = 0
 * mprotect(0x7fb1f604c000, 16384, PROT_READ) = 0
 * mprotect(0x55e0b86ea000, 4096, PROT_READ) = 0
 * mprotect(0x7fb1f627f000, 4096, PROT_READ) = 0
 * munmap(0x7fb1f6277000, 31819)           = 0
 * rt_sigaction(SIGINT, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8) =
 *0 rt_sigaction(SIGINT, {sa_handler=0x55e0b84d2630, sa_mask=[],
 *sa_flags=SA_RESTORER, sa_restorer=0x7fb1f5ca4040}, NULL, 8) = 0
 * rt_sigaction(SIGQUIT, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8)
 *= 0 rt_sigaction(SIGQUIT, {sa_handler=0x55e0b84d2630, sa_mask=[],
 *sa_flags=SA_RESTORER, sa_restorer=0x7fb1f5ca4040}, NULL, 8) = 0
 * rt_sigaction(SIGPIPE, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8)
 *= 0 rt_sigaction(SIGPIPE, {sa_handler=0x55e0b84d2630, sa_mask=[],
 *sa_flags=SA_RESTORER, sa_restorer=0x7fb1f5ca4040}, NULL, 8) = 0
 * rt_sigaction(SIGUSR1, NULL, {sa_handler=SIG_DFL, sa_mask=[], sa_flags=0}, 8)
 *= 0 rt_sigaction(SIGUSR1, {sa_handler=0x55e0b84d25b0, sa_mask=[],
 *sa_flags=SA_RESTORER, sa_restorer=0x7fb1f5ca4040}, NULL, 8) = 0
 * stat("/dev/sg0", {st_mode=S_IFCHR|0660, st_rdev=makedev(21, 0), ...}) = 0
 * openat(AT_FDCWD, "/dev/sg0", O_RDWR)    = 3
 * stat("/dev/sg2", {st_mode=S_IFCHR|0660, st_rdev=makedev(21, 2), ...}) = 0
 * openat(AT_FDCWD, "/dev/sg2", O_RDWR)    = 4
 * brk(NULL)                               = 0x55e0ba246000
 * brk(0x55e0ba267000)                     = 0x55e0ba267000
 * ioctl(3, SG_GET_VERSION_NUM, [30536])   = 0
 * ioctl(3, SG_SET_RESERVED_SIZE, [4096])  = 0
 * ioctl(3, SG_GET_SCSI_ID, {host_no=0, channel=0, scsi_id=0, lun=0,
 *scsi_type=0, h_cmd_per_lun=2048, d_queue_depth=2048}) = 0 ioctl(4,
 *SG_GET_VERSION_NUM, [30536])   = 0 ioctl(4, SG_SET_RESERVED_SIZE, [4096])  = 0
 * ioctl(4, SG_GET_SCSI_ID, {host_no=2, channel=0, scsi_id=0, lun=0,
 *scsi_type=0, h_cmd_per_lun=2048, d_queue_depth=2048}) = 0 openat(AT_FDCWD,
 *"/dev/sg0", O_RDWR)    = 5 ioctl(5, SG_GET_VERSION_NUM, [30536])   = 0
 * ioctl(5, SG_SET_RESERVED_SIZE, [4096])  = 0
 * ioctl(5, SG_GET_SCSI_ID, {host_no=0, channel=0, scsi_id=0, lun=0,
 *scsi_type=0, h_cmd_per_lun=2048, d_queue_depth=2048}) = 0 openat(AT_FDCWD,
 *"/dev/sg2", O_RDWR)    = 6 ioctl(6, SG_GET_VERSION_NUM, [30536])   = 0
 * ioctl(6, SG_SET_RESERVED_SIZE, [4096])  = 0
 * ioctl(6, SG_GET_SCSI_ID, {host_no=2, channel=0, scsi_id=0, lun=0,
 *scsi_type=0, h_cmd_per_lun=2048, d_queue_depth=2048}) = 0 openat(AT_FDCWD,
 *"/dev/sg0", O_RDWR)    = 7 ioctl(7, SG_GET_VERSION_NUM, [30536])   = 0
 * ioctl(7, SG_SET_RESERVED_SIZE, [4096])  = 0
 * ioctl(7, SG_GET_SCSI_ID, {host_no=0, channel=0, scsi_id=0, lun=0,
 *scsi_type=0, h_cmd_per_lun=2048, d_queue_depth=2048}) = 0 openat(AT_FDCWD,
 *"/dev/sg2", O_RDWR)    = 8 ioctl(8, SG_GET_VERSION_NUM, [30536])   = 0
 * ioctl(8, SG_SET_RESERVED_SIZE, [4096])  = 0
 * ioctl(8, SG_GET_SCSI_ID, {host_no=2, channel=0, scsi_id=0, lun=0,
 *scsi_type=0, h_cmd_per_lun=2048, d_queue_depth=2048}) = 0 openat(AT_FDCWD,
 *"/dev/sg0", O_RDWR)    = 9 ioctl(9, SG_GET_VERSION_NUM, [30536])   = 0
 * ioctl(9, SG_SET_RESERVED_SIZE, [4096])  = 0
 * ioctl(9, SG_GET_SCSI_ID, {host_no=0, channel=0, scsi_id=0, lun=0,
 *scsi_type=0, h_cmd_per_lun=2048, d_queue_depth=2048}) = 0 openat(AT_FDCWD,
 *"/dev/sg2", O_RDWR)    = 10 ioctl(10, SG_GET_VERSION_NUM, [30536])  = 0
 * ioctl(10, SG_SET_RESERVED_SIZE, [4096]) = 0
 * ioctl(10, SG_GET_SCSI_ID, {host_no=2, channel=0, scsi_id=0, lun=0,
 *scsi_type=0, h_cmd_per_lun=2048, d_queue_depth=2048}) = 0 write(3,
 *"S\0\0\0\375\377\377\377\n
 *\0\0\0\20\0\0\0p$\272\340U\0\0\340b$\272\340U\0\0"..., 88) = 88 poll([{fd=4,
 *events=POLLIN}, {fd=6, events=POLLIN}, {fd=8, events=POLLIN}, {fd=10,
 *events=POLLIN}], 4, 0) = 0 (Timeout) poll([{fd=3, events=POLLIN}, {fd=5,
 *events=POLLIN}, {fd=7, events=POLLIN}, {fd=9, events=POLLIN}], 4, 0) = 1
 *([{fd=3, revents=POLLIN}]) read(3, "S\0\0\0\375\377\377\377\n
 *\0\0\0\20\0\0\0p$\272\340U\0\0\340b$\272\340U\0\0"..., 88) = 88 write(4,
 *"S\0\0\0\376\377\377\377\n
 *\0\0\0\20\0\0\0p$\272\340U\0\0\340b$\272\340U\0\0"..., 88) = 88 poll([{fd=4,
 *events=POLLIN}, {fd=6, events=POLLIN}, {fd=8, events=POLLIN}, {fd=10,
 *events=POLLIN}], 4, 0) = 0 (Timeout) poll([{fd=3, events=POLLIN}, {fd=5,
 *events=POLLIN}, {fd=7, events=POLLIN}, {fd=9, events=POLLIN}], 4, 0) = 0
 *(Timeout) poll([{fd=4, events=POLLIN}, {fd=6, events=POLLIN}, {fd=8,
 *events=POLLIN}, {fd=10, events=POLLIN}], 4, 0) = 0 (Timeout) poll([{fd=3,
 *events=POLLIN}, {fd=5, events=POLLIN}, {fd=7, events=POLLIN}, {fd=9,
 *events=POLLIN}], 4, 0) = 0 (Timeout) poll([{fd=4, events=POLLIN}, {fd=6,
 *events=POLLIN}, {fd=8, events=POLLIN}, {fd=10, events=POLLIN}], 4, 0) = 0
 *(Timeout) poll([{fd=3, events=POLLIN}, {fd=5, events=POLLIN}, {fd=7,
 *events=POLLIN}, {fd=9, events=POLLIN}], 4, 0) = 0 (Timeout) poll([{fd=4,
 *events=POLLIN}, {fd=6, events=POLLIN}, {fd=8, events=POLLIN}, {fd=10,
 *events=POLLIN}], 4, 0) = 0 (Timeout) poll([{fd=3, events=POLLIN}, {fd=5,
 *events=POLLIN}, {fd=7, events=POLLIN}, {fd=9, events=POLLIN}], 4, 0) = 0
 *(Timeout) poll([{fd=4, events=POLLIN}, {fd=6, events=POLLIN}, {fd=8,
 *events=POLLIN}, {fd=10, events=POLLIN}], 4, 0) = 1 ([{fd=4, revents=POLLIN}])
 * read(4, "S\0\0\0\376\377\377\377\n
 *\0\0\0\20\0\0\0p$\272\340U\0\0\340b$\272\340U\0\0"..., 88) = 88 close(3) = 0
 * close(4)                                = 0
 * write(2, "1+0 records in\n", 151+0 records in
 * )        = 15
 * write(2, "1+0 records out\n", 161+0 records out
 * )       = 16
 * write(2, ">> Non-zero sum of residual coun"..., 40>> Non-zero sum of residual
 *counts=3584 ) = 40 exit_group(0)                           = ?
 * +++ exited with 0 +++
 ***/

#define _XOPEN_SOURCE 500

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/major.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
typedef uint8_t u_char; /* horrible, for scsi.h */
#include "sg_io_linux.h"
#include "sg_lib.h"
#include "sg_unaligned.h"

static char *version_str = "0.63 20190324";
/* resurrected from "0.55 20020509" */

#define DEF_BLOCK_SIZE 512
#define DEF_BLOCKS_PER_TRANSFER 128

#define SENSE_BUFF_LEN 32 /* Arbitrary, could be larger */
#define DEF_TIMEOUT 60000 /* 60,000 millisecs == 60 seconds */
#define S_RW_LEN 10       /* Use SCSI READ(10) and WRITE(10) */

#define SGP_READ10 0x28
#define SGP_WRITE10 0x2a
#define DEF_NUM_THREADS 4 /* actually degree of concurrency */
#define MAX_NUM_THREADS 1024

#ifndef RAW_MAJOR
#define RAW_MAJOR 255 /*unlikey value */
#endif

#define FT_OTHER 0 /* filetype other than sg or raw device */
#define FT_SG 1    /* filetype is sg char device */
#define FT_RAW 2   /* filetype is raw char device */

#define QS_IDLE 0        /* ready to start a copy cycle */
#define QS_IN_STARTED 1  /* commenced read */
#define QS_IN_FINISHED 2 /* finished read, ready for write */
#define QS_OUT_STARTED 3 /* commenced write */
#define QS_IN_POLL 11
#define QS_OUT_POLL 12

static void print_state_name(int x) {
  if (x == QS_IDLE) {
    printf("QS_IDLE\n");
  } else if (x == QS_IN_STARTED) {
    printf("QS_IN_STARTED\n");
  } else if (x == QS_IN_FINISHED) {
    printf("QS_IN_FINISHED\n");
  } else if (x == QS_OUT_STARTED) {
    printf("QS_OUT_STARTED\n");
  } else if (x == QS_IN_POLL) {
    printf("QS_IN_POLL\n");
  } else if (x == QS_OUT_POLL) {
    printf("QS_OUT_POLL\n");
  }
}

#define STR_SZ 1024
#define INOUTF_SZ 512
#define EBUFF_SZ 512

struct request_element;

typedef struct request_collection { /* one instance visible to all threads */
  int infd;
  int skip;
  int in_type;
  int in_scsi_type;
  int in_blk;        /* next block address to read */
  int in_count;      /* blocks remaining for next read */
  int in_done_count; /* count of completed in blocks */
  int in_partial;
  int outfd;
  int seek;
  int out_type;
  int out_scsi_type;
  int out_blk;        /* next block address to write */
  int out_count;      /* blocks remaining for next write */
  int out_done_count; /* count of completed out blocks */
  int out_partial;
  int bs;
  int bpt;
  int dio;
  int dio_incomplete;
  int sum_of_resids;
  int coe;
  int debug;
  int num_rq_elems;
  struct request_element *req_arr;
} Rq_coll;

typedef struct request_element { /* one instance per worker thread */
  int qstate;                    /* "QS" state */
  int infd;
  int outfd;
  int wr;
  int blk;
  int num_blks;
  uint8_t *buffp;
  uint8_t *alloc_bp;
  sg_io_hdr_t io_hdr;
  uint8_t cmd[S_RW_LEN];
  uint8_t sb[SENSE_BUFF_LEN];
  int bs;
  int dio;
  int dio_incomplete;
  int resid;
  int in_scsi_type;
  int out_scsi_type;
  int debug;
} Rq_elem;

static Rq_coll rcoll;
static struct pollfd in_pollfd_arr[MAX_NUM_THREADS];
static struct pollfd out_pollfd_arr[MAX_NUM_THREADS];
static int dd_count = -1;

static const char *proc_allow_dio = "/proc/scsi/sg/allow_dio";

static int sg_finish_io(int wr, Rq_elem *rep);

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

static void print_stats() {
  int infull, outfull;

  if (0 != rcoll.out_count)
    fprintf(stderr, "  remaining block count=%d\n", rcoll.out_count);
  infull = dd_count - rcoll.in_done_count - rcoll.in_partial;
  fprintf(stderr, "%d+%d records in\n", infull, rcoll.in_partial);
  outfull = dd_count - rcoll.out_done_count - rcoll.out_partial;
  fprintf(stderr, "%d+%d records out\n", outfull, rcoll.out_partial);
}

static void interrupt_handler(int sig) {
  struct sigaction sigact;

  sigact.sa_handler = SIG_DFL;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = 0;
  sigaction(sig, &sigact, NULL);
  fprintf(stderr, "Interrupted by signal,");
  print_stats();
  kill(getpid(), sig);
}

static void siginfo_handler(int sig) {
  fprintf(stderr, "Progress report, continuing ...\n");
  print_stats();
  if (sig) {
  } /* suppress unused warning */
}

static int dd_filetype(const char *filename) {
  struct stat st;

  if (stat(filename, &st) < 0)
    return FT_OTHER;
  if (S_ISCHR(st.st_mode)) {
    if (RAW_MAJOR == major(st.st_rdev))
      return FT_RAW;
    else if (SCSI_GENERIC_MAJOR == major(st.st_rdev))
      return FT_SG;
  }
  return FT_OTHER;
}

static void usage() {
  fprintf(stderr,
          "Usage: "
          "sgq_dd  [if=<infile>] [skip=<n>] [of=<ofile>] [seek=<n>] "
          "[bs=<num>]\n"
          "            [bpt=<num>] [count=<n>] [dio=0|1] [thr=<n>] "
          "[coe=0|1] [gen=<n>]\n"
          "            [time=0|1] [deb=<n>] [--version]\n"
          "         usually either 'if' or 'of' is a sg or raw device\n"
          " 'bpt' is blocks_per_transfer (default is 128)\n"
          " 'dio' is direct IO, 1->attempt, 0->indirect IO (def)\n"
          " 'thr' is number of queues, must be > 0, default 4, max 1024\n");
  fprintf(stderr,
          " 'coe' continue on sg error, 0->exit (def), "
          "1->zero + continue\n"
          " 'time' 0->no timing(def), 1->time plus calculate throughput\n"
          " 'gen' 0-> 1 file is special(def), 1-> any files allowed\n"
          " 'deb' is debug, 0->none (def), > 0->varying degrees of debug\n");
}

/* Returns -1 for error, 0 for nothing found, QS_IN_POLL or QS_OUT_POLL */
static int do_poll(Rq_coll *clp, int timeout, int *req_indexp) {
  int k, res;

  if (FT_SG == clp->out_type) {
    while (((res = poll(out_pollfd_arr, clp->num_rq_elems, timeout)) < 0) &&
           (EINTR == errno))
      ;

    assert(res >= 0);

    if (res > 0) {
      for (k = 0; k < clp->num_rq_elems; ++k) {
        if (out_pollfd_arr[k].revents & POLLIN) {
          if (req_indexp) {
            *req_indexp = k;
          }
          return QS_OUT_POLL;
        }
      }
    }
  }

  if (FT_SG == clp->in_type) {
    while (((res = poll(in_pollfd_arr, clp->num_rq_elems, timeout)) < 0) &&
           (EINTR == errno))
      ;

    assert(res >= 0);

    if (res > 0) {
      for (k = 0; k < clp->num_rq_elems; ++k) {
        if (in_pollfd_arr[k].revents & POLLIN) {
          if (req_indexp)
            *req_indexp = k;
          return QS_IN_POLL;
        }
      }
    }
  }
  return 0;
}

/* Return of 0 -> success, -1 -> failure, 2 -> try again */
static int read_capacity(int sg_fd, int *num_sect, int *sect_sz) {
  int res;
  uint8_t rc_cdb[10] = {0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint8_t rcBuff[64];
  uint8_t sense_b[64];
  sg_io_hdr_t io_hdr;

  memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
  io_hdr.interface_id = 'S';
  io_hdr.cmd_len = sizeof(rc_cdb);
  io_hdr.mx_sb_len = sizeof(sense_b);
  io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
  io_hdr.dxfer_len = sizeof(rcBuff);
  io_hdr.dxferp = rcBuff;
  io_hdr.cmdp = rc_cdb;
  io_hdr.sbp = sense_b;
  io_hdr.timeout = DEF_TIMEOUT;

  if (ioctl(sg_fd, SG_IO, &io_hdr) < 0) {
    perror("read_capacity (SG_IO) error");
    return -1;
  }

  res = sg_err_category3(&io_hdr);
  if (SG_LIB_CAT_UNIT_ATTENTION == res)
    return 2; /* probably have another go ... */
  else if (SG_LIB_CAT_CLEAN != res) {
    sg_chk_n_print3("read capacity", &io_hdr, 1);
    return -1;
  }

  *num_sect = 1 + sg_get_unaligned_be32(rcBuff + 0);
  *sect_sz = sg_get_unaligned_be32(rcBuff + 4);

#ifdef DEBUG
  fprintf(stderr, "number of sectors=%d, sector size=%d\n", *num_sect,
          *sect_sz);
#endif

  return 0;
}

static int sg_start_io(Rq_elem *rep) {
  sg_io_hdr_t *hp = &rep->io_hdr;
  int res;

  rep->qstate = rep->wr ? QS_OUT_STARTED : QS_IN_STARTED;
  memset(rep->cmd, 0, sizeof(rep->cmd));
  rep->cmd[0] = rep->wr ? SGP_WRITE10 : SGP_READ10;
  sg_put_unaligned_be32((uint32_t)rep->blk, rep->cmd + 2);
  sg_put_unaligned_be16((uint16_t)rep->num_blks, rep->cmd + 7);
  memset(hp, 0, sizeof(sg_io_hdr_t));
  hp->interface_id = 'S';
  hp->cmd_len = sizeof(rep->cmd);
  hp->cmdp = rep->cmd;
  hp->dxfer_direction = rep->wr ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
  hp->dxfer_len = rep->bs * rep->num_blks;
  hp->dxferp = rep->buffp;
  hp->mx_sb_len = sizeof(rep->sb);
  hp->sbp = rep->sb;
  hp->timeout = DEF_TIMEOUT;
  hp->usr_ptr = rep;
  hp->pack_id = rep->blk;

  if (rep->dio)
    hp->flags |= SG_FLAG_DIRECT_IO;

  while (((res = write(rep->wr ? rep->outfd : rep->infd, hp,
                       sizeof(sg_io_hdr_t))) < 0) &&
         (EINTR == errno))
    ;

  assert(res >= 0);
  return 0;
}

/* -1 -> unrecoverable error, 0 -> successful, 1 -> try again */
static int sg_finish_io(int wr, Rq_elem *rep) {
  int res;
  sg_io_hdr_t io_hdr;
  sg_io_hdr_t *hp;

  memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
  io_hdr.interface_id = 'S';
  io_hdr.dxfer_direction = rep->wr ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
  io_hdr.pack_id = rep->blk;

  while (((res = read(wr ? rep->outfd : rep->infd, &io_hdr,
                      sizeof(sg_io_hdr_t))) < 0) &&
         (EINTR == errno))
    ;

  memcpy(&rep->io_hdr, &io_hdr, sizeof(sg_io_hdr_t));
  hp = &rep->io_hdr;

  assert(sg_err_category3(hp) == SG_LIB_CAT_CLEAN);

  // 我们并不需要这个!
  if (rep->dio && ((hp->info & SG_INFO_DIRECT_IO_MASK) != SG_INFO_DIRECT_IO))
    rep->dio_incomplete = 1; /* count dios done as indirect IO */
  else {
    rep->dio_incomplete = 0;
  }
  return 0;
}

/* Returns scsi_type or -1 for error */
static int sg_prepare(int fd, int sz) {
  int res, t;
  struct sg_scsi_id info;

  res = ioctl(fd, SG_GET_VERSION_NUM, &t);
  if ((res < 0) || (t < 30000)) {
    fprintf(stderr, "sgq_dd: sg driver prior to 3.x.y\n");
    return -1;
  }
  res = ioctl(fd, SG_SET_RESERVED_SIZE, &sz);
  if (res < 0)
    perror("sgq_dd: SG_SET_RESERVED_SIZE error");
#if 0
    t = 1;
    res = ioctl(fd, SG_SET_FORCE_PACK_ID, &t);
    if (res < 0)
        perror("sgq_dd: SG_SET_FORCE_PACK_ID error");
#endif
  res = ioctl(fd, SG_GET_SCSI_ID, &info);
  if (res < 0) {
    perror("sgq_dd: SG_SET_SCSI_ID error");
    return -1;
  } else
    return info.scsi_type;
}

/* Return 0 for ok, anything else for errors */
static int prepare_rq_elems(Rq_coll *clp, const char *inf, const char *outf) {
  int k;
  Rq_elem *rep;
  size_t psz;
  char ebuff[EBUFF_SZ];
  int sz = clp->bpt * clp->bs;
  int scsi_type;

  clp->req_arr = malloc(sizeof(Rq_elem) * clp->num_rq_elems);
  if (NULL == clp->req_arr)
    return 1;
  for (k = 0; k < clp->num_rq_elems; ++k) {
    rep = &clp->req_arr[k];
    memset(rep, 0, sizeof(Rq_elem));
    psz = getpagesize();
    if (NULL == (rep->alloc_bp = malloc(sz + psz)))
      return 1;
    rep->buffp =
        (uint8_t *)(((unsigned long)rep->alloc_bp + psz - 1) & (~(psz - 1)));
    rep->qstate = QS_IDLE;
    rep->bs = clp->bs;
    rep->dio = clp->dio;
    rep->debug = clp->debug;
    rep->out_scsi_type = clp->out_scsi_type;
    if (FT_SG == clp->in_type) {
      if (0 == k)
        rep->infd = clp->infd;
      else {
        if ((rep->infd = open(inf, O_RDWR)) < 0) {
          snprintf(ebuff, EBUFF_SZ, "sgq_dd: could not open %s for sg reading",
                   inf);
          perror(ebuff);
          return 1;
        }
      }
      in_pollfd_arr[k].fd = rep->infd;
      in_pollfd_arr[k].events = POLLIN;
      if ((scsi_type = sg_prepare(rep->infd, sz)) < 0)
        return 1;
      if (0 == k)
        clp->in_scsi_type = scsi_type;
      rep->in_scsi_type = clp->in_scsi_type;
    } else
      rep->infd = clp->infd;

    if (FT_SG == clp->out_type) {
      if (0 == k)
        rep->outfd = clp->outfd;
      else {
        if ((rep->outfd = open(outf, O_RDWR)) < 0) {
          snprintf(ebuff, EBUFF_SZ, "sgq_dd: could not open %s for sg writing",
                   outf);
          perror(ebuff);
          return 1;
        }
      }
      out_pollfd_arr[k].fd = rep->outfd;
      out_pollfd_arr[k].events = POLLIN;
      if ((scsi_type = sg_prepare(rep->outfd, sz)) < 0)
        return 1;
      if (0 == k)
        clp->out_scsi_type = scsi_type;
      rep->out_scsi_type = clp->out_scsi_type;
    } else
      rep->outfd = clp->outfd;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  bool verbose_given = false;
  bool version_given = false;
  int skip = 0;
  int seek = 0;
  int ibs = 0;
  int obs = 0;
  char str[STR_SZ];
  char *key;
  char *buf;
  char inf[INOUTF_SZ];
  char outf[INOUTF_SZ];
  int res, k, n, keylen;
  int in_num_sect = 0;
  int out_num_sect = 0;
  int num_threads = DEF_NUM_THREADS;
  int gen = 0;
  int do_time = 0;
  int in_sect_sz, out_sect_sz;
  int qstate, req_index, seek_skip;
  int blocks, stop_after_write, terminate;
  char ebuff[EBUFF_SZ];
  Rq_elem *rep;
  struct timeval start_tm, end_tm;

  memset(&rcoll, 0, sizeof(Rq_coll));
  rcoll.bpt = DEF_BLOCKS_PER_TRANSFER;
  rcoll.in_type = FT_OTHER;
  rcoll.out_type = FT_OTHER;
  inf[0] = '\0';
  outf[0] = '\0';

  for (k = 1; k < argc; k++) {
    if (argv[k])
      strncpy(str, argv[k], STR_SZ);
    else
      continue;
    for (key = str, buf = key; *buf && *buf != '=';)
      buf++;
    if (*buf)
      *buf++ = '\0';
    keylen = strlen(key);
    if (strcmp(key, "if") == 0)
      strncpy(inf, buf, INOUTF_SZ);
    else if (strcmp(key, "of") == 0)
      strncpy(outf, buf, INOUTF_SZ);
    else if (0 == strcmp(key, "ibs"))
      ibs = sg_get_num(buf);
    else if (0 == strcmp(key, "obs"))
      obs = sg_get_num(buf);
    else if (0 == strcmp(key, "bs"))
      rcoll.bs = sg_get_num(buf);
    else if (0 == strcmp(key, "bpt"))
      rcoll.bpt = sg_get_num(buf);
    else if (0 == strcmp(key, "skip"))
      skip = sg_get_num(buf);
    else if (0 == strcmp(key, "seek"))
      seek = sg_get_num(buf);
    else if (0 == strcmp(key, "count"))
      dd_count = sg_get_num(buf);
    else if (0 == strcmp(key, "dio"))
      rcoll.dio = sg_get_num(buf);
    else if (0 == strcmp(key, "thr"))
      num_threads = sg_get_num(buf);
    else if (0 == strcmp(key, "coe"))
      rcoll.coe = sg_get_num(buf);
    else if (0 == strcmp(key, "gen"))
      gen = sg_get_num(buf);
    else if ((0 == strncmp(key, "deb", 3)) || (0 == strncmp(key, "verb", 4)))
      rcoll.debug = sg_get_num(buf);
    else if (0 == strcmp(key, "time"))
      do_time = sg_get_num(buf);
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
      rcoll.debug += n;
      res += n;
      n = num_chs_in_str(key + 1, keylen - 1, 'V');
      if (n > 0)
        version_given = true;
      res += n;
      if (res < (keylen - 1)) {
        fprintf(stderr,
                "Unrecognised short option in '%s', try "
                "'--help'\n",
                key);
        return SG_LIB_SYNTAX_ERROR;
      }
    } else if (0 == strncmp(key, "--help", 6)) {
      usage();
      return 0;
    } else if (0 == strncmp(key, "--verb", 6)) {
      verbose_given = true;
      ++rcoll.debug;
    } else if (0 == strncmp(key, "--vers", 6))
      version_given = true;
    else {
      fprintf(stderr, "Unrecognized argument '%s'\n", key);
      usage();
      return 1;
    }
  }
#ifdef DEBUG
  fprintf(stderr, "In DEBUG mode, ");
  if (verbose_given && version_given) {
    fprintf(stderr, "but override: '-vV' given, zero verbose and "
                    "continue\n");
    verbose_given = false;
    version_given = false;
    rcoll.debug = 0;
  } else if (!verbose_given) {
    fprintf(stderr, "set '-vv'\n");
    rcoll.debug = 2;
  } else
    fprintf(stderr, "keep verbose=%d\n", rcoll.debug);
#else
  if (verbose_given && version_given)
    fprintf(stderr, "Not in DEBUG mode, so '-vV' has no special action\n");
#endif
  if (version_given) {
    fprintf(stderr, "sgq_dd for sg version 3 driver: %s\n", version_str);
    return 0;
    return 0;
  }

  if (argc < 2) {
    usage();
    return 1;
  }
  if (rcoll.bs <= 0) {
    rcoll.bs = DEF_BLOCK_SIZE;
    fprintf(stderr, "Assume default 'bs' (block size) of %d bytes\n", rcoll.bs);
  }
  if ((ibs && (ibs != rcoll.bs)) || (obs && (obs != rcoll.bs))) {
    fprintf(stderr, "If 'ibs' or 'obs' given must be same as 'bs'\n");
    usage();
    return 1;
  }
  if ((skip < 0) || (seek < 0)) {
    fprintf(stderr, "skip and seek cannot be negative\n");
    return 1;
  }
  if ((num_threads < 1) || (num_threads > MAX_NUM_THREADS)) {
    fprintf(stderr, "too few or too many threads requested\n");
    usage();
    return 1;
  }
  if (rcoll.debug)
    fprintf(stderr, "sgq_dd: if=%s skip=%d of=%s seek=%d count=%d\n", inf, skip,
            outf, seek, dd_count);
  install_handler(SIGINT, interrupt_handler);
  install_handler(SIGQUIT, interrupt_handler);
  install_handler(SIGPIPE, interrupt_handler);
  install_handler(SIGUSR1, siginfo_handler);

  rcoll.infd = STDIN_FILENO;
  rcoll.outfd = STDOUT_FILENO;
  if (inf[0] && ('-' != inf[0])) {
    rcoll.in_type = dd_filetype(inf);

    if (FT_SG == rcoll.in_type) {
      if ((rcoll.infd = open(inf, O_RDWR)) < 0) {
        snprintf(ebuff, EBUFF_SZ, "sgq_dd: could not open %s for sg reading",
                 inf);
        perror(ebuff);
        return 1;
      }
    }
    if (FT_SG != rcoll.in_type) {
      if ((rcoll.infd = open(inf, O_RDONLY)) < 0) {
        snprintf(ebuff, EBUFF_SZ, "sgq_dd: could not open %s for reading", inf);
        perror(ebuff);
        return 1;
      } else if (skip > 0) {
        loff_t offset = skip;

        offset *= rcoll.bs; /* could exceed 32 here! */
        if (lseek(rcoll.infd, offset, SEEK_SET) < 0) {
          snprintf(ebuff, EBUFF_SZ,
                   "sgq_dd: couldn't skip to required position on %s", inf);
          perror(ebuff);
          return 1;
        }
      }
    }
  }
  if (outf[0] && ('-' != outf[0])) {
    rcoll.out_type = dd_filetype(outf);

    if (FT_SG == rcoll.out_type) {
      if ((rcoll.outfd = open(outf, O_RDWR)) < 0) {
        snprintf(ebuff, EBUFF_SZ, "sgq_dd: could not open %s for sg writing",
                 outf);
        perror(ebuff);
        return 1;
      }
    } else {
      if (FT_OTHER == rcoll.out_type) {
        if ((rcoll.outfd = open(outf, O_WRONLY | O_CREAT, 0666)) < 0) {
          snprintf(ebuff, EBUFF_SZ, "sgq_dd: could not open %s for writing",
                   outf);
          perror(ebuff);
          return 1;
        }
      } else {
        if ((rcoll.outfd = open(outf, O_WRONLY)) < 0) {
          snprintf(ebuff, EBUFF_SZ, "sgq_dd: could not open %s for raw writing",
                   outf);
          perror(ebuff);
          return 1;
        }
      }
      if (seek > 0) {
        loff_t offset = seek;

        offset *= rcoll.bs; /* could exceed 32 bits here! */
        if (lseek(rcoll.outfd, offset, SEEK_SET) < 0) {
          snprintf(ebuff, EBUFF_SZ,
                   "sgq_dd: couldn't seek to required position on %s", outf);
          perror(ebuff);
          return 1;
        }
      }
    }
  }
  if ((STDIN_FILENO == rcoll.infd) && (STDOUT_FILENO == rcoll.outfd)) {
    fprintf(stderr, "Disallow both if and of to be stdin and stdout\n");
    return 1;
  }
  if ((FT_OTHER == rcoll.in_type) && (FT_OTHER == rcoll.out_type) && !gen) {
    fprintf(stderr, "Either 'if' or 'of' must be a sg or raw device\n");
    return 1;
  }
  if (0 == dd_count)
    return 0;
  else if (dd_count < 0) {
    if (FT_SG == rcoll.in_type) {
      res = read_capacity(rcoll.infd, &in_num_sect, &in_sect_sz);
      if (2 == res) {
        fprintf(stderr, "Unit attention, media changed(in), repeat\n");
        res = read_capacity(rcoll.infd, &in_num_sect, &in_sect_sz);
      }
      if (0 != res) {
        fprintf(stderr, "Unable to read capacity on %s\n", inf);
        in_num_sect = -1;
      } else {
        if (in_num_sect > skip)
          in_num_sect -= skip;
      }
    }
    if (FT_SG == rcoll.out_type) {
      res = read_capacity(rcoll.outfd, &out_num_sect, &out_sect_sz);
      if (2 == res) {
        fprintf(stderr, "Unit attention, media changed(out), "
                        "repeat\n");
        res = read_capacity(rcoll.outfd, &out_num_sect, &out_sect_sz);
      }
      if (0 != res) {
        fprintf(stderr, "Unable to read capacity on %s\n", outf);
        out_num_sect = -1;
      } else {
        if (out_num_sect > seek)
          out_num_sect -= seek;
      }
    }
    if (in_num_sect > 0) {
      if (out_num_sect > 0)
        dd_count = (in_num_sect > out_num_sect) ? out_num_sect : in_num_sect;
      else
        dd_count = in_num_sect;
    } else
      dd_count = out_num_sect;
  }
  if (rcoll.debug > 1)
    fprintf(stderr,
            "Start of loop, count=%d, in_num_sect=%d, "
            "out_num_sect=%d\n",
            dd_count, in_num_sect, out_num_sect);
  if (dd_count <= 0) {
    fprintf(stderr, "Couldn't calculate count, please give one\n");
    return 1;
  }

  rcoll.in_count = dd_count;
  rcoll.in_done_count = dd_count;
  rcoll.skip = skip;
  rcoll.in_blk = skip;
  rcoll.out_count = dd_count;
  rcoll.out_done_count = dd_count;
  rcoll.seek = seek;
  rcoll.out_blk = seek;

  if ((FT_SG == rcoll.in_type) || (FT_SG == rcoll.out_type))
    rcoll.num_rq_elems = num_threads;
  else
    rcoll.num_rq_elems = 1;
  if (prepare_rq_elems(&rcoll, inf, outf)) {
    fprintf(stderr, "Setup failure, perhaps no memory\n");
    return 1;
  }

  stop_after_write = 0;
  terminate = 0;
  seek_skip = rcoll.seek - rcoll.skip;
  if (do_time) {
    start_tm.tv_sec = 0;
    start_tm.tv_usec = 0;
    gettimeofday(&start_tm, NULL);
  }

  printf("input use SG = %d\n", FT_SG == rcoll.in_type);
  printf("output use SG = %d\n", FT_SG == rcoll.out_type);

  int access_count = 0;
  int pre_state = -1;
  sg_io_hdr_t *hp = NULL;


  int read_package_id = 0;
  int write_package_id = 0;

  // 这里我们统一下工作的流程
  // Read
  // poll
  // sg_finish_io -> 看一下读完没有

  // Write
  // poll
  // sg_finish_io
  while (rcoll.out_done_count > 0) {
    uint8_t cmd[10];
    uint8_t sb[32];

    read_package_id++;
    write_package_id++;

    printf("item to write = %d\n", rcoll.out_done_count);

    rep = (rcoll.req_arr + req_index);
    // read
    {
      // 这里先是读数据
      blocks = 1;
      rep->wr = 0; // 这里是读
      rep->blk = rcoll.in_blk;
      rep->num_blks = blocks;
      rcoll.in_blk++;
      rcoll.in_count--;

      // 这里会去改变状态
      // QS_OUT_STARTED : QS_IN_STARTED;
      hp = &rep->io_hdr;
      rep->qstate = QS_IN_STARTED;

      memset(cmd, 0, sizeof(cmd));
      cmd[0] = rep->wr ? SGP_WRITE10 : SGP_READ10;
      sg_put_unaligned_be32((uint32_t)rep->blk, cmd + 2);
      sg_put_unaligned_be16((uint16_t)rep->num_blks, cmd + 7);

      memset(hp, 0, sizeof(sg_io_hdr_t));
      hp->interface_id = 'S';
      hp->cmd_len = sizeof(cmd);
      hp->cmdp = cmd;
      hp->dxfer_direction = rep->wr ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
      hp->dxfer_len = rep->bs * rep->num_blks;
      hp->dxferp = rep->buffp;
      hp->mx_sb_len = sizeof(sb);
      hp->sbp = sb;
      hp->timeout = DEF_TIMEOUT;
      hp->usr_ptr = NULL;
      hp->pack_id = read_package_id;

      if (rep->dio) {
        hp->flags |= SG_FLAG_DIRECT_IO;
      }

      while (((res = write(rep->wr ? rep->outfd : rep->infd, hp,
                           sizeof(sg_io_hdr_t))) < 0) &&
             (EINTR == errno))
        ;
      assert(res >= 0);
    }

    // wait
    {
      // 如果读完之后
      // 这里需要poll
      // 那么我们就poll一下。
      while ((((res = poll(in_pollfd_arr, 1, 0)) < 0) && (EINTR == errno)) ||
             (0 == res))
        ;
      assert(res > 0);
      assert(in_pollfd_arr[0].revents & POLLIN);
      req_index = 0;
      qstate = QS_IN_POLL;
    }

    // verify read over
    {
      rep->qstate = QS_IN_FINISHED;
      res = sg_finish_io(rep->wr, rep);
      rcoll.in_done_count--;
      assert(res == 0);
    }

    // write
    {
      // 这里开始写!
      rep->wr = 1;
      rep->blk = rcoll.out_blk;
      blocks = rep->num_blks;
      rcoll.out_blk += blocks;
      rcoll.out_count -= blocks;
      // 这里会去改变状态
      // QS_OUT_STARTED : QS_IN_STARTED;
      hp = &rep->io_hdr;
      rep->qstate = QS_IN_STARTED;

      memset(rep->cmd, 0, sizeof(rep->cmd));
      rep->cmd[0] = rep->wr ? SGP_WRITE10 : SGP_READ10;
      sg_put_unaligned_be32((uint32_t)rep->blk, rep->cmd + 2);
      sg_put_unaligned_be16((uint16_t)rep->num_blks, rep->cmd + 7);

      memset(hp, 0, sizeof(sg_io_hdr_t));
      hp->interface_id = 'S';
      hp->cmd_len = sizeof(rep->cmd);
      hp->cmdp = rep->cmd;
      hp->dxfer_direction = rep->wr ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
      hp->dxfer_len = rep->bs * rep->num_blks;
      hp->dxferp = rep->buffp;
      hp->mx_sb_len = sizeof(rep->sb);
      hp->sbp = rep->sb;
      hp->timeout = DEF_TIMEOUT;
      hp->usr_ptr = NULL;
      hp->pack_id = rep->blk;

      if (rep->dio) {
        hp->flags |= SG_FLAG_DIRECT_IO;
      }

      while (((res = write(rep->wr ? rep->outfd : rep->infd, hp,
                           sizeof(sg_io_hdr_t))) < 0) &&
             (EINTR == errno))
        ;
      assert(res > 0);
    }

    // poll
    {
      while ((((res = poll(out_pollfd_arr, 1, 0)) < 0) && (EINTR == errno)) ||
             (0 == res))
        ;
      assert(res > 0);
      assert(out_pollfd_arr[0].revents & POLLIN);
      req_index = 0;
      qstate = QS_OUT_POLL;
    }

    // wait write over
    {
      // 等写完
      rep->qstate = QS_IDLE;
      res = sg_finish_io(rep->wr, rep);
      rcoll.out_done_count--;
      assert(res == 0);
    }
  }

  if ((do_time) && (start_tm.tv_sec || start_tm.tv_usec)) {
    struct timeval res_tm;
    double a, b;

    gettimeofday(&end_tm, NULL);
    res_tm.tv_sec = end_tm.tv_sec - start_tm.tv_sec;
    res_tm.tv_usec = end_tm.tv_usec - start_tm.tv_usec;
    if (res_tm.tv_usec < 0) {
      --res_tm.tv_sec;
      res_tm.tv_usec += 1000000;
    }
    a = res_tm.tv_sec;
    a += (0.000001 * res_tm.tv_usec);
    b = (double)rcoll.bs * (dd_count - rcoll.out_done_count);
    printf("time to transfer data was %d.%06d secs", (int)res_tm.tv_sec,
           (int)res_tm.tv_usec);
    if ((a > 0.00001) && (b > 511))
      printf(", %.2f MB/sec\n", b / (a * 1000000.0));
    else
      printf("\n");
  }

  if (STDIN_FILENO != rcoll.infd)
    close(rcoll.infd);
  if (STDOUT_FILENO != rcoll.outfd)
    close(rcoll.outfd);
  res = 0;
  if (0 != rcoll.out_count) {
    fprintf(stderr, ">>>> Some error occurred,\n");
    res = 2;
  }
  print_stats();
  if (rcoll.dio_incomplete) {
    int fd;
    char c;

    fprintf(stderr, ">> Direct IO requested but incomplete %d times\n",
            rcoll.dio_incomplete);
    if ((fd = open(proc_allow_dio, O_RDONLY)) >= 0) {
      if (1 == read(fd, &c, 1)) {
        if ('0' == c)
          fprintf(stderr,
                  ">>> %s set to '0' but should be set "
                  "to '1' for direct IO\n",
                  proc_allow_dio);
      }
      close(fd);
    }
  }
  if (rcoll.sum_of_resids)
    fprintf(stderr, ">> Non-zero sum of residual counts=%d\n",
            rcoll.sum_of_resids);
  return res;
}
