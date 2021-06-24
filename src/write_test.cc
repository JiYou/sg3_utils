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

#include <bits/stdc++.h>

#ifndef RAW_MAJOR
#define RAW_MAJOR 255 /*unlikely value */
#endif

#define FT_OTHER 1  /* filetype other than sg and ... */
#define FT_SG 2     /* filetype is sg char device */
#define FT_RAW 4    /* filetype is raw char device */
#define FT_BLOCK 8  /* filetype is block device */
#define FT_ERROR 64 /* couldn't "stat" file */

constexpr uint32_t KiB = 1024;
constexpr uint32_t MiB = KiB * KiB;
constexpr uint32_t k4KiB = KiB << 2;

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
  const int total_write_times = 10;

  // 这里我们要开始计时
  auto start_total_time = std::chrono::system_clock::now();

  auto get_diff_ns = [](const decltype(start_total_time)& start,
                        const decltype(start_total_time)& end) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    // 1,000 nanoseconds – one microsecond
    // 1,000 microseconds - one ms
  };

  for (int i = 0; i < total_write_times; i++) {
    // 到这里我们开始写
  }

  auto end_total_time = std::chrono::system_clock::now();

  // 停止计时！

  free(write_buf);
  close(fd);

  return 0;
}