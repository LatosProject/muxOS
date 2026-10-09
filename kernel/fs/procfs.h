#include <stdint.h>

struct inode;
struct dirent;
#define PROC_INO_BASE 100
enum proc_entry {
  PROC_DOT,
  PROC_DOTDOT,
  PROC_CPUINFO,
  PROC_MEMINFO,
  PROC_LOADAVG,
  PROC_VERSION,
  PROC_ENTRY_COUNT
};
void fs_procfs_init(void);

int procfs_read(struct inode *ip, void *buf, uint32_t off, uint32_t count);
int procfs_readdir(struct inode *ip, uint32_t *cookie, struct dirent *entry);
uint32_t procfs_lookup(const char *name);
