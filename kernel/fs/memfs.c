/*
 * memfs.c - in-memory inode/content backend.
 *
 * Adapted from xv6's fs.c (MIT License, Copyright (c) 2006-2024 Frans
 * Kaashoek, Robert Morris, Russ Cox, MIT).  The block device, buffer cache,
 * journal, superblock and on-disk inode table are gone: an inode's data
 * "blocks" are simply physical pages from pmm_alloc(), which vmm_init()
 * identity-maps, so they can be dereferenced as normal pointers.
 */

#include "./procfs.h"
#include "fs.h"
#include "fs_uapi.h"
#include "pmm.h"
#include "programs.h"
#include "string.h"
#include "vga.h"
#include <stdint.h>

/* One inode per slot, indexed by inum; inum 0 means "unused". */
static struct inode inode_table[MAX_INODES];

/*
 * 根据 inode 编号取得内存中的 inode。
 *
 * @inum: inode 编号；0 保留为“无效编号”。
 *
 * inode_table 使用编号作为数组下标，因此这里只做范围检查和槽位有效性
 * 检查，不增加引用计数，也不会分配新 inode。成功返回 inode 指针；编号
 * 越界或对应槽位未使用（type == 0）时返回 NULL。
 */
struct inode *iget(uint32_t inum) {
  if (inum == 0 || inum >= MAX_INODES)
    return 0;
  struct inode *ip = &inode_table[inum];
  if (ip->type == 0)
    return 0;
  return ip;
}

/*
 * 从全局 inode 表中分配一个空闲 inode。
 *
 * @type:  T_DIR、T_FILE 或 T_DEVICE。
 * @major: 设备主编号；普通文件和目录通常传 0。
 * @minor: 设备次编号；普通文件和目录通常传 0。
 *
 * 分配时会清空旧内容，设置编号、类型、设备号、初始链接数和默认权限。
 * 成功返回新 inode；inode 表已满时返回 NULL。该函数只创建 inode 本身，
 * 不会自动把它链接到任何目录。
 */
struct inode *ialloc(int type, int major, int minor) {
  for (uint32_t i = 1; i < MAX_INODES; i++) {
    if (inode_table[i].type == 0) {
      struct inode *ip = &inode_table[i];
      kmemset(ip, 0, sizeof(*ip));
      ip->inum = i;
      ip->type = type;
      ip->major = major;
      ip->minor = minor;
      ip->nlink = 1;
      ip->mode = (type == T_DIR)      ? MODE_DIR
                 : (type == T_DEVICE) ? MODE_DEV
                                      : MODE_FILE;
      return ip;
    }
  }
  return 0;
}

/*
 * 清空 inode 的全部文件内容。
 *
 * 释放 addrs[] 中记录的所有物理页，并把文件大小重置为 0。inode 的编号、
 * 类型、权限和链接数保持不变，因此该 inode 清空后仍然有效。
 */
void itrunc(struct inode *ip) {
  for (uint32_t i = 0; i < NDIRECT; i++) {
    if (ip->addrs[i]) {
      pmm_free(ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }
  ip->size = 0;
}

/*
 * 彻底释放一个 inode 槽位。
 *
 * 先通过 itrunc() 释放内容页，再用 type == 0 标记槽位为空。调用者必须先
 * 确认目录项和打开文件不再需要该 inode；本函数本身不检查引用关系。
 */
void ifree(struct inode *ip) {
  itrunc(ip);
  ip->type = 0;
  ip->nlink = 0;
}

/*
 * 返回 inode 第 bn 个数据块对应的物理页地址。
 *
 * 如果该块尚未分配，则从 PMM 分配并清零一个 4 KiB 页，再记录到
 * ip->addrs[bn]。块号超过 NDIRECT 或分配失败时返回 0。
 *
 * 注意：该函数用于写入或扩容，会产生实际的页分配；纯读取不应调用它，
 * 否则读取空洞也会消耗物理内存。
 */
static uint32_t bmap(struct inode *ip, uint32_t bn) {
  if (bn >= NDIRECT)
    return 0;
  if (ip->addrs[bn] == 0) {
    uint32_t page = pmm_alloc();
    if (!page)
      return 0;
    kmemset((void *)(uintptr_t)page, 0, BSIZE);
    ip->addrs[bn] = page;
  }
  return ip->addrs[bn];
}

/*
 * 从 inode 内容中读取最多 n 个字节。
 *
 * @ip:  要读取的 inode。
 * @dst: 内核可写的目标缓冲区。
 * @off: 文件内起始字节偏移。
 * @n:   请求读取的字节数。
 *
 * 请求超过文件末尾时会自动截短；off 位于文件末尾之外时返回 0。函数按
 * BSIZE 分块复制，返回实际读取字节数。当前接口不负责验证用户指针，调用
 * 者必须保证 dst 可访问。
 */
int readi(struct inode *ip, void *dst, uint32_t off, uint32_t n) {
  /* 9p files are read per open-file description (see fileread), and 9p
   * directories are served from the memfs-format listing v9p_loaddir()
   * built, so everything reaching here is memfs-backed. */
  if (off > ip->size)
    return 0;
  if (off + n > ip->size)
    n = ip->size - off;

  uint32_t tot = 0;
  while (tot < n) {
    uint32_t bn = (off + tot) / BSIZE;
    uint32_t bo = (off + tot) % BSIZE;
    uint32_t m = BSIZE - bo;
    if (m > n - tot)
      m = n - tot;
    uint32_t addr = ip->addrs[bn];
    if (addr == 0)
      break;
    kmemcpy((uint8_t *)dst + tot, (void *)(uintptr_t)(addr + bo), m);
    tot += m;
  }
  return (int)tot;
}

/*
 * 向 inode 内容写入最多 n 个字节。
 *
 * @ip:  要写入的 inode。
 * @src: 源缓冲区。
 * @off: 文件内起始字节偏移。
 * @n:   请求写入的字节数。
 *
 * memfs 当前不支持从文件末尾之后开始写入，因此 off > ip->size 会失败；
 * 写入范围也不能超过 MAXFILE。缺失的数据块由 bmap() 按需分配。返回实际
 * 写入字节数，参数越界时返回 -1；发生中途分配失败时可能返回短写结果。
 */
int writei(struct inode *ip, const void *src, uint32_t off, uint32_t n) {
  if (off > ip->size || off + n > MAXFILE)
    return -1;

  uint32_t tot = 0;
  while (tot < n) {
    uint32_t bn = (off + tot) / BSIZE;
    uint32_t bo = (off + tot) % BSIZE;
    uint32_t m = BSIZE - bo;
    if (m > n - tot)
      m = n - tot;
    uint32_t addr = bmap(ip, bn);
    if (addr == 0)
      break;
    kmemcpy((void *)(uintptr_t)(addr + bo), (const uint8_t *)src + tot, m);
    tot += m;
  }
  if (off + tot > ip->size)
    ip->size = off + tot;
  return (int)tot;
}

/*
 * 将 inode 调整为指定字节数。
 *
 * @ip:   要调整的 inode。
 * @size: 新文件大小，不能超过 MAXFILE。
 *
 * 扩大文件时分配缺失的数据页，并把新暴露的区间清零；缩小时释放完全落在
 * 新文件末尾之后的整页。成功返回 0，大小非法或扩容分配失败时返回 -1。
 */
int itruncate(struct inode *ip, uint32_t size) {
  if (size > MAXFILE)
    return -1;
  if (size < ip->size) {
    uint32_t first = (size + BSIZE - 1) / BSIZE;
    for (uint32_t bn = first; bn < NDIRECT; bn++) {
      if (ip->addrs[bn]) {
        pmm_free(ip->addrs[bn]);
        ip->addrs[bn] = 0;
      }
    }
  } else if (size > ip->size) {
    for (uint32_t off = ip->size; off < size;) {
      uint32_t addr = bmap(ip, off / BSIZE);
      if (addr == 0)
        return -1;
      uint32_t bo = off % BSIZE;
      uint32_t span = BSIZE - bo;
      if (span > size - off)
        span = size - off;
      kmemset((void *)(uintptr_t)(addr + bo), 0, span);
      off += span;
    }
  }
  ip->size = size;
  return 0;
}

/*
 * 比较两个目录项名称。
 *
 * 目录项名称最多占 DIRSIZ 字节，因此比较被限制在 DIRSIZ 范围内，避免
 * 读取越过 struct dirent.name。
 */
static int namecmp(const char *a, const char *b) {
  return kstrncmp(a, b, DIRSIZ);
}

/*
 * 在目录 inode 中查找指定名称。
 *
 * @dp:   目录 inode。
 * @name: 要查找的单个路径分量。
 * @poff: 可选输出参数；找到时写入该目录项在目录数据中的字节偏移。
 *
 * 普通 memfs 目录的内容是一组连续 struct dirent。函数逐项读取，跳过
 * inum == 0 的空槽，找到后返回子节点的 inode 编号；未找到或读取失败时
 * 返回 0。对于 9P 目录，会先处理虚拟的 "."/".."，并按需加载目录列表。
 */
int dirlookup(struct inode *dp, const char *name, uint32_t *poff) {
  if (dp->backend == INODE_9P) {
    /* The host tree has no "." / ".." entries; synthesize them. */
    if (kstrcmp(name, ".") == 0) {
      if (poff)
        *poff = 0;
      return (int)dp->inum;
    }
    if (kstrcmp(name, "..") == 0) {
      if (poff)
        *poff = 0;
      return (int)(dp->parent ? dp->parent : dp->inum);
    }
    if (!dp->dir_loaded)
      v9p_loaddir(dp);
  }
  if (dp->backend == INODE_PROCFS) {
    extern uint32_t proc_inum[4];
    if (kstrcmp(name, ".") == 0) {
      if (poff)
        *poff = 0;
      return (int)dp->inum;
    }
    if (kstrcmp(name, "..") == 0) {
      if (poff)
        *poff = 0;
      return (int)(dp->parent ? dp->parent : dp->inum);
    }
    if (kstrcmp(name, "cpuinfo") == 0)
      return proc_inum[0];
    if (kstrcmp(name, "meminfo") == 0)
      return proc_inum[1];
    if (kstrcmp(name, "loadavg") == 0)
      return proc_inum[2];
    if (kstrcmp(name, "version") == 0)
      return proc_inum[3];
    return 0;
  }

  struct dirent de;
  for (uint32_t off = 0; off < dp->size; off += sizeof(de)) {
    if (readi(dp, &de, off, sizeof(de)) != (int)sizeof(de))
      return 0;
    if (de.inum == 0)
      continue;
    if (namecmp(name, de.name) == 0) {
      if (poff)
        *poff = off;
      return (int)de.inum;
    }
  }
  return 0;
}

/*
 * 在目录中建立 name -> inum 的目录项。
 *
 * 若名称已存在则失败。写入时优先复用 inum == 0 的空槽，没有空槽时追加
 * 到目录末尾；目录项类型取自目标 inode。成功返回 0，重复名称、目录读取
 * 失败或写入失败时返回 -1。
 *
 * 本函数只增加目录项，不会自动修改目标 inode 的 nlink；需要增加硬链接
 * 计数时由上层调用者负责。
 */
int dirlink(struct inode *dp, const char *name, uint32_t inum) {
  if (dirlookup(dp, name, 0))
    return -1;

  struct dirent de;
  uint32_t off = 0;
  for (; off < dp->size; off += sizeof(de)) {
    if (readi(dp, &de, off, sizeof(de)) != (int)sizeof(de))
      return -1;
    if (de.inum == 0)
      break;
  }

  kmemset(&de, 0, sizeof(de));
  de.inum = inum;
  struct inode *target = iget(inum);
  de.type = target ? (uint16_t)target->type : 0;
  uint32_t i = 0;
  while (name[i] && i < DIRSIZ - 1) {
    de.name[i] = name[i];
    i++;
  }
  de.name[i] = 0;

  if (writei(dp, &de, off, sizeof(de)) != (int)sizeof(de))
    return -1;
  return 0;
}

/*
 * 从目录中移除指定名称。
 *
 * 实现方式是找到对应目录项后将整个 struct dirent 清零，保留该槽位供后续
 * dirlink() 复用。成功返回被移除条目的 inode 编号；名称不存在或写回失败
 * 时返回 -1。该函数不会释放 inode，也不会调整 nlink。
 */
int dirunlink(struct inode *dp, const char *name) {
  uint32_t off;
  uint32_t inum = (uint32_t)dirlookup(dp, name, &off);
  if (inum == 0)
    return -1;
  struct dirent de;
  kmemset(&de, 0, sizeof(de));
  if (writei(dp, &de, off, sizeof(de)) != (int)sizeof(de))
    return -1;
  return (int)inum;
}

/*
 * 判断目录是否为空。
 *
 * 空槽以及 "."、".." 不计为实际内容。没有其他有效目录项时返回 1，发现
 * 任意其他条目时返回 0。该检查主要用于 rmdir。
 */
int isdirempty(struct inode *ip) {
  struct dirent de;
  for (uint32_t off = 0; off < ip->size; off += sizeof(de)) {
    if (readi(ip, &de, off, sizeof(de)) != (int)sizeof(de))
      break;
    if (de.inum == 0)
      continue;
    if (kstrcmp(de.name, ".") == 0 || kstrcmp(de.name, "..") == 0)
      continue;
    return 0;
  }
  return 1;
}

/*
 * 把内核 inode 元数据转换为用户 ABI 使用的 struct stat。
 *
 * 这里只复制 inode 编号、类型、权限、链接数、大小和设备号，不读取文件
 * 内容。调用者必须提供有效且可写的 st 缓冲区。
 */
void stati(struct inode *ip, struct stat *st) {
  st->ino = ip->inum;
  st->type = (uint16_t)ip->type;
  st->mode = ip->mode;
  st->nlink = (uint16_t)ip->nlink;
  st->size = ip->size;
  st->dev = 0;
}

/*
 * 创建 inode 并把它链接到指定目录。
 *
 * 这是 fs_init() 使用的内部辅助函数。先调用 ialloc() 创建 inode，再通过
 * dirlink() 写入父目录；如果链接失败，会回滚并释放刚创建的 inode。
 * 成功返回新 inode，失败返回 NULL。
 */
static struct inode *create(struct inode *dir, const char *name, int type,
                            int major) {
  struct inode *ip = ialloc(type, major, 0);
  if (!ip)
    return 0;
  if (dirlink(dir, name, ip->inum) < 0) {
    ifree(ip);
    return 0;
  }
  return ip;
}

int readdir(struct inode *dir, uint32_t *off, struct dirent *entry) {

  if (dir->type != T_DIR) {
    return -ENOTDIR;
  }

  struct dirent de;
  while (*off + sizeof(de) <= dir->size) {
    if (readi(dir, &de, *off, sizeof(de)) != (int)sizeof(de))
      return -EIO;

    *off += sizeof(de);

    if (de.inum == 0)
      continue;

    de.off = *off;

    kmemcpy(entry, &de, sizeof(de));
    return 1;
  }
  return 0;
}
/*
 * 初始化启动时的内存文件系统。
 *
 * 初始化 inode/open-file/设备表，构造根目录、/dev、/etc、/root 和 /bin，
 * 再把链接进内核镜像的用户程序复制到 /bin。Toybox applet 通过多个目录项
 * 共享同一个 inode，实现类似硬链接的 multicall 布局。
 *
 * 该函数应在 PMM/VMM 和终端初始化完成后调用，并且只能初始化一次；重复
 * 调用会清空 inode_table，使已有 inode 和打开文件全部失效。
 */
void fs_init(void) {
  kmemset(inode_table, 0, sizeof(inode_table));
  fileinit();
  devsw_init();

  /* Root inode is pinned to ROOTINO. */
  struct inode *root = &inode_table[ROOTINO];
  root->inum = ROOTINO;
  root->type = T_DIR;
  root->nlink = 2;
  root->mode = MODE_DIR;

  dirlink(root, ".", ROOTINO);
  dirlink(root, "..", ROOTINO);

  struct inode *dev = create(root, "dev", T_DIR, 0);
  if (dev) {
    dev->nlink = 2;
    dirlink(dev, ".", dev->inum);
    dirlink(dev, "..", ROOTINO);
    root->nlink++;
    create(dev, "console", T_DEVICE, CONSOLE_MAJOR);
    create(dev, "tty", T_DEVICE, CONSOLE_MAJOR);
    create(dev, "null", T_DEVICE, NULL_MAJOR);
  }

  /* /etc/passwd + /etc/group so getpwuid()/id/whoami work. */
  struct inode *etc = create(root, "etc", T_DIR, 0);
  if (etc) {
    etc->nlink = 2;
    dirlink(etc, ".", etc->inum);
    dirlink(etc, "..", ROOTINO);
    root->nlink++;
    struct inode *pw = create(etc, "passwd", T_FILE, 0);
    if (pw) {
      const char *s = "root:x:0:0:root:/:/bin/sh\n";
      writei(pw, s, 0, kstrlen(s));
    }
    struct inode *gr = create(etc, "group", T_FILE, 0);
    if (gr) {
      const char *s = "root:x:0:\n";
      writei(gr, s, 0, kstrlen(s));
    }
  }

  struct inode *hello = create(root, "hello", T_FILE, 0);
  if (hello) {
    const char *msg = "Hello from the muxOS memfs!\n";
    writei(hello, msg, 0, kstrlen(msg));
  }

  /* Mount point for the host directory passed via QEMU -virtfs.  v9p_mount()
   * turns this into a 9p-backed directory if a device is present. */
  struct inode *hostroot = create(root, "root", T_DIR, 0);
  if (hostroot)
    hostroot->nlink = 2;

  /* Copy every embedded program into /bin so the shell can exec it. */
  struct inode *bin = create(root, "bin", T_DIR, 0);
  if (bin) {
    bin->nlink = 2;
    dirlink(bin, ".", bin->inum);
    dirlink(bin, "..", ROOTINO);
    root->nlink++;
    struct inode *toybox = 0;
    for (uint32_t i = 0; i < embedded_program_count; i++) {
      const struct embedded_program *p = &embedded_programs[i];
      uint32_t size = (uint32_t)(p->end - p->start);
      struct inode *f = create(bin, p->name, T_FILE, 0);
      if (!f || size == 0)
        continue;
      if (size > MAXFILE)
        size = MAXFILE;
      writei(f, p->start, 0, size);
      if (kstrcmp(p->name, "toybox") == 0)
        toybox = f;
    }
    /* Every applet name is a hardlink to the one multicall binary. */
    if (toybox) {
      for (uint32_t i = 0; i < embedded_applet_count; i++) {
        if (dirlink(bin, embedded_applets[i], toybox->inum) == 0)
          toybox->nlink++;
      }
    }
  }

  print("[OK] FS init\n", 0);
}

/*
 * 执行启动阶段的 memfs 冒烟测试。
 *
 * 测试通过公开 VFS 接口完成文件创建、写入、重新打开、读取和内容比较，
 * 同时验证 mkdir/stat/rmdir。测试产生的 /selftest 和 /selftest_dir 会在结束
 * 前删除，结果输出到控制台（并镜像到 COM1）。失败只打印诊断信息，不会
 * panic 或中止后续启动。
 */
void fs_selftest(void) {
  const char *path = "/selftest";
  const char *payload = "muxOS memfs payload 12345";

  struct file *f = vfs_open(path, O_CREAT | O_RDWR | O_TRUNC);
  if (!f) {
    print("[FS] selftest: create failed\n", 0x0C);
    return;
  }
  filewrite(f, payload, (int)kstrlen(payload));
  fileclose(f);

  f = vfs_open(path, O_RDONLY);
  if (!f) {
    print("[FS] selftest: open failed\n", 0x0C);
    return;
  }
  char buf[64];
  int n = fileread(f, buf, sizeof(buf) - 1);
  fileclose(f);
  if (n < 0)
    n = 0;
  buf[n] = 0;
  int rw_ok = kstrcmp(buf, payload) == 0;

  int dir_ok = 0;
  vfs_mkdir("/selftest_dir");
  struct inode *ip = namei("/selftest_dir");
  if (ip) {
    struct stat st;
    stati(ip, &st);
    dir_ok = (st.type == T_DIR);
  }

  vfs_unlink("/selftest");
  vfs_rmdir("/selftest_dir");

  if (rw_ok && dir_ok) {
    print("[FS] selftest PASS\n", 0x0A);
  } else {
    print("[FS] selftest FAIL (rw=", 0x0C);
    print(rw_ok ? "ok" : "bad", 0x0C);
    print(", dir=", 0x0C);
    print(dir_ok ? "ok" : "bad", 0x0C);
    print(")\n", 0x0C);
  }
}
