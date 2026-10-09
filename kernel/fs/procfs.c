
#include "procfs.h"
#include "../kernel/lib/stdio.h"
#include "../kernel/mm/pmm.h"
#include "fs.h"
#include "fs_uapi.h"
#include "kernel.h"
#include "process.h"
#include "string.h"
#include <stdint.h>
#include <cpuid.h>
#define PROC_INO_BASE 100
#define PROC_VERSION_LEN 16
#define PROC_MEMINFO_LEN 512
#define PROC_CPUINFO_LEN 1536
static char cpuinfo_buf[PROC_CPUINFO_LEN];
uint32_t proc_inum[4];

/* These describe CPUID capabilities, not which features the kernel enables. */
static void cpuinfo_init(void) {
  uint32_t eax, ebx, ecx, edx;
  uint32_t max_basic = __get_cpuid_max(0, 0);
  char vendor[13] = "unknown";
  char brand[49] = "unknown";
  char flags[512];
  flags[0] = '\0';
  uint32_t family = 0, model = 0, stepping = 0;

  if (max_basic) {
    __cpuid(0, eax, ebx, ecx, edx);
    kmemcpy(vendor, &ebx, 4);
    kmemcpy(vendor + 4, &edx, 4);
    kmemcpy(vendor + 8, &ecx, 4);
    vendor[12] = '\0';

    __cpuid(1, eax, ebx, ecx, edx);
    uint32_t base_family = (eax >> 8) & 15;
    family = base_family;
    model = (eax >> 4) & 15;
    stepping = eax & 15;
    if (base_family == 15)
      family += (eax >> 20) & 255;
    if (base_family == 6 || base_family == 15)
      model |= ((eax >> 16) & 15) << 4;

    static const struct {
      uint32_t bit;
      const char *name;
    } edx_flags[] = {
        {0, "fpu"}, {1, "vme"}, {2, "de"}, {3, "pse"}, {4, "tsc"},
        {5, "msr"}, {6, "pae"}, {7, "mce"}, {8, "cx8"}, {9, "apic"},
        {11, "sep"}, {12, "mtrr"}, {13, "pge"}, {14, "mca"},
        {15, "cmov"}, {16, "pat"}, {17, "pse36"}, {19, "clflush"},
        {23, "mmx"}, {24, "fxsr"}, {25, "sse"}, {26, "sse2"},
        {28, "ht"}},
      ecx_flags[] = {
        {0, "pni"}, {1, "pclmulqdq"}, {9, "ssse3"}, {13, "cx16"},
        {19, "sse4_1"}, {20, "sse4_2"}, {22, "movbe"}, {23, "popcnt"},
        {25, "aes"}, {26, "xsave"}, {30, "rdrand"}, {31, "hypervisor"}};

    for (uint32_t i = 0; i < sizeof(edx_flags) / sizeof(edx_flags[0]); i++) {
      if (edx & (1u << edx_flags[i].bit)) {
        uint32_t len = kstrlen(flags);
        ksnprintf(flags + len, sizeof(flags) - len, "%s%s",
                  len ? " " : "", edx_flags[i].name);
      }
    }
    for (uint32_t i = 0; i < sizeof(ecx_flags) / sizeof(ecx_flags[0]); i++) {
      if (ecx & (1u << ecx_flags[i].bit)) {
        uint32_t len = kstrlen(flags);
        ksnprintf(flags + len, sizeof(flags) - len, "%s%s",
                  len ? " " : "", ecx_flags[i].name);
      }
    }

    if (__get_cpuid_max(0x80000000u, 0) >= 0x80000004u) {
      for (uint32_t i = 0; i < 3; i++) {
        __cpuid(0x80000002u + i, eax, ebx, ecx, edx);
        kmemcpy(brand + i * 16, &eax, 4);
        kmemcpy(brand + i * 16 + 4, &ebx, 4);
        kmemcpy(brand + i * 16 + 8, &ecx, 4);
        kmemcpy(brand + i * 16 + 12, &edx, 4);
      }
      brand[48] = '\0';
    }
  }

  const char *name = brand;
  while (*name == ' ')
    name++;
  /* SMP is not initialized: expose only the CPU actually used by muxOS. */
  ksnprintf(cpuinfo_buf, sizeof(cpuinfo_buf),
            "processor\t: 0\n"
            "vendor_id\t: %s\n"
            "cpu family\t: %u\n"
            "model\t\t: %u\n"
            "model name\t: %s\n"
            "stepping\t: %u\n"
            "flags\t\t: %s\n\n",
            vendor, family, model, *name ? name : "unknown", stepping, flags);
}

/* proc files end at their text length, not at the buffer capacity. */
static int procfs_read_text(const char *text, void *buf, uint32_t off,
                            uint32_t count) {
  uint32_t len = kstrlen(text);
  if (off >= len)
    return 0;
  uint32_t n = len - off;
  if (n > count)
    n = count;
  kmemcpy(buf, text + off, n);
  return (int)n;
}

void fs_procfs_init(void) {
  cpuinfo_init();
  vfs_mkdir("/proc");
  struct inode *proc = namei("/proc");
  if (!proc)
    return;
  proc->backend = INODE_PROCFS;
  proc->parent = ROOTINO;
  for (int i = 0; i < PROC_ENTRY_COUNT - PROC_DOTDOT; i++) {
    struct inode *ip = ialloc(T_FILE, 0, 0);
    if (!ip)
      return;
    ip->backend = INODE_PROCFS;
    ip->parent = proc->inum;
    proc_inum[i] = ip->inum;
  }
}

int procfs_read(struct inode *ip, void *buf, uint32_t off, uint32_t count) {
  if (ip->type != T_FILE) {
    return -1;
  }
  if (ip->backend != INODE_PROCFS) {
    return -1;
  }
  if (proc_inum[0] == ip->inum) {
    // CPUINFO
    return procfs_read_text(cpuinfo_buf, buf, off, count);
  } else if (proc_inum[1] == ip->inum) {
    // MEMINFO
    if (off >= PROC_MEMINFO_LEN) {
      return 0;
    }
    uint32_t n = PROC_MEMINFO_LEN - off;
    if (n > count)
      n = count;
    static char meminfo_buf[PROC_MEMINFO_LEN] = {0};
    memory_info_t info;
    pmm_get_info(&info);
    ksnprintf(meminfo_buf, PROC_MEMINFO_LEN,
              "MemTotal:       %u kB\n"
              "MemFree:        %u kB\n"
              "MemUsed:        %u kB\n",
              (uint32_t)(info.total / 1024), (uint32_t)(info.free / 1024),
              (uint32_t)(info.used / 1024));
    ;
    kmemcpy(buf, meminfo_buf + off, n);
    return (int)n;
  } else if (proc_inum[2] == ip->inum) {
    // LOADAVG
    process_loadavg_t info;
    char text[128];
    process_get_loadavg(&info);
    ksnprintf(text, sizeof(text), "%u.%s%u %u.%s%u %u.%s%u %u/%u %u\n",
              info.avg[0] / 100, info.avg[0] % 100 < 10 ? "0" : "",
              info.avg[0] % 100,
              info.avg[1] / 100, info.avg[1] % 100 < 10 ? "0" : "",
              info.avg[1] % 100,
              info.avg[2] / 100, info.avg[2] % 100 < 10 ? "0" : "",
              info.avg[2] % 100,
              info.runnable, info.total, info.last_pid);
    return procfs_read_text(text, buf, off, count);
  } else if (proc_inum[3] == ip->inum) {
    // VERSION
    static char version_buf[PROC_VERSION_LEN] = KERNEL_VERSION;
    if (off >= PROC_VERSION_LEN) {
      return 0;
    }
    uint32_t n = PROC_VERSION_LEN - off;
    if (n > count)
      n = count;

    kmemcpy(buf, version_buf + off, n);
    return (int)n;
  }

  return 0;
}
int procfs_readdir(struct inode *dir, uint32_t *cookie, struct dirent *entry) {
  if (dir->type != T_DIR) {
    return -1;
  }
  if (dir->backend != INODE_PROCFS) {
    return -1;
  }

  kmemset(entry, 0, sizeof(*entry));
  switch (*cookie) {
  case PROC_DOT:
    entry->inum = dir->inum;
    kstrcpy(entry->name, ".");
    entry->type = T_DIR;
    break;
  case PROC_DOTDOT:
    entry->inum = ROOTINO;
    kstrcpy(entry->name, "..");
    entry->type = T_DIR;
    break;
  case PROC_CPUINFO:
    entry->inum = proc_inum[0];
    kstrcpy(entry->name, "cpuinfo");
    entry->off = *cookie + 1;
    entry->type = T_FILE;
    break;
  case PROC_MEMINFO:
    entry->inum = proc_inum[1];
    kstrcpy(entry->name, "meminfo");
    entry->off = *cookie + 1;
    entry->type = T_FILE;
    break;
  case PROC_LOADAVG:
    entry->inum = proc_inum[2];
    kstrcpy(entry->name, "loadavg");
    entry->off = *cookie + 1;
    entry->type = T_FILE;
    break;
  case PROC_VERSION:
    entry->inum = proc_inum[3];
    kstrcpy(entry->name, "version");
    entry->off = *cookie + 1;
    entry->type = T_FILE;
    break;
  default:
    goto process_entry;
  }
  entry->off = *cookie + 1;
  (*cookie)++;
  return 1;

process_entry:
  uint32_t index = *cookie - PROC_ENTRY_COUNT;
  process_info_t info;
  uint32_t process_count = (uint32_t)process_get_count();
  if (index < process_count) {
    uint32_t pid = processes[index].pid;
    if (process_get_info(pid, &info) < 0) {
      return -1;
    }
  } else {
    return 0;
  }
  entry->inum = PROC_INO_BASE + info.pid;
  char buf[16];
  kitoa((int32_t)info.pid, buf, 10);
  kstrcpy(entry->name, buf);
  entry->off = *cookie + 1;
  entry->type = T_DIR;
  (*cookie)++;
  return 1;
}
