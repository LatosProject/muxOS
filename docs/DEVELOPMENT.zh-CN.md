# muxOS 开发文档

本文面向准备阅读、调试或扩展 muxOS 的开发者。内容以当前仓库代码为准，最后核对日期为 2026 年 9 月 27 日。

## 1. 项目定位

muxOS 是一个运行在 32 位 x86（i686）上的实验性操作系统。内核主要使用 C 和 NASM 汇编编写，通过 GRUB Multiboot 启动，在 QEMU 中运行。用户程序是静态链接的 ELF32/i386 可执行文件，C 运行时和标准库由项目内的 mlibc 移植层提供。

当前仓库包含：

- 基础 x86 启动、GDT、TSS、IDT、PIC 和 PIT 支持；
- 物理页分配与二级页表管理；
- 内核线程、用户进程、抢占式调度、`fork`、`execve`、`waitpid` 和基础信号处理；
- memfs、设备文件、管道，以及通过 virtio-9p 挂载宿主目录；
- VGA/Framebuffer 终端、PS/2 键盘和串口输出；
- mlibc 移植，以及 mrsh、Toybox、neatvi、sfm 等用户程序；
- Nix 构建环境，可在 `x86_64-linux` 和 Apple Silicon macOS（`aarch64-darwin`）上交叉构建。

这仍是教学和实验用途的内核，不应假设它具备完整的 POSIX 语义、多核安全性或生产环境所需的隔离能力。

## 2. 快速开始

### 2.1 推荐方式：Nix 构建完整系统

仓库的 Nix flake 会依次构建交叉工具链、mlibc、用户程序和最终 ISO。完整构建时，`/bin` 会包含 shell、Toybox、vi 和 sfm。

```sh
nix --extra-experimental-features 'nix-command flakes' build
```

构建产物位于：

```text
result/muxos.iso
```

启动系统：

```sh
nix --extra-experimental-features 'nix-command flakes' run
```

flake 默认将当前宿主目录通过 virtio-9p 挂载到 muxOS 的 `/root`。可以用 `MUXOS_SHARE` 指定其他目录：

```sh
MUXOS_SHARE="$PWD" nix --extra-experimental-features 'nix-command flakes' run
```

需要向 QEMU 追加参数时，将参数放在 `--` 之后。例如把串口输出重定向到当前终端：

```sh
nix --extra-experimental-features 'nix-command flakes' run -- --serial stdio
```

### 2.2 进入开发环境

```sh
nix --extra-experimental-features 'nix-command flakes' develop
make
make run
```

开发环境提供 i686 交叉编译器、binutils、NASM、GRUB、QEMU、Meson、Ninja 和 pkg-config。

> 注意：裸 `make` 默认只构建内核和 ISO，`PROGRAMS` 为空，因此生成的 memfs 中通常没有 `/bin/sh`。要得到可交互的完整系统，优先使用 `nix build`/`nix run`，或者显式传入已经为 muxOS 构建好的用户程序。

### 2.3 直接使用 Makefile

主 Makefile 的常用目标如下：

| 命令 | 作用 |
| --- | --- |
| `make` | 构建 `build/kernel.elf` 和 `build/muxos.iso` |
| `make run` | 构建后使用 QEMU 启动 |
| `make clean` | 删除 `build/` |
| `make print-sources` | 输出被纳入内核的 C/汇编源文件 |
| `make compile-commands` | 重新生成 clangd 使用的 `compile_commands.json` |

Makefile 会自动收集 `arch/`、`kernel/` 和 `drivers/` 下的 C 源文件。新增源文件后一般不需要手工维护对象列表。

## 3. 仓库结构

```text
.
├── arch/x86/                 # 启动、描述符表、中断和上下文切换
├── drivers/
│   ├── bus/                  # PCI 枚举
│   ├── input/                # PS/2 键盘
│   ├── platform/             # 重启、关机、停机
│   ├── serial/               # COM1 串口
│   ├── video/                # VGA 文本和线性 framebuffer
│   └── virtio/               # legacy virtio-9p 传输
├── kernel/
│   ├── fs/                   # VFS、memfs、devfs、9P、procfs
│   ├── lib/                  # freestanding 字符串/内存函数
│   ├── mm/                   # PMM 和 VMM
│   ├── task/                 # 进程、调度、信号和系统调用
│   ├── tty/                  # 终端状态机和转义序列解析
│   ├── elf.c                 # ELF32/i386 加载器
│   └── kernel.c              # 内核初始化入口
├── ports/mlibc/              # mlibc 的 muxOS sysdeps 和链接脚本
├── nix/pkgs/                 # 内核、mlibc、shell 和应用的 Nix 包
├── iso/                      # GRUB ISO 相关文件
├── flake.nix                 # 完整构建图和运行入口
├── Makefile                  # 内核/ISO 的底层构建规则
└── linker.ld                 # 内核链接布局，加载地址从 1 MiB 开始
```

## 4. 系统启动流程

启动主链路如下：

```text
GRUB Multiboot
    │
    ▼
arch/x86/boot.s::_start
    │  设置 16 KiB 引导栈，传入 magic 和 multiboot_info
    ▼
kernel/kernel.c::kernel_main
    │
    ├─ GDT / TSS
    ├─ PIC / PIT（1000 Hz）/ IDT
    ├─ 串口 / framebuffer / terminal
    ├─ PMM / VMM / keyboard
    ├─ memfs / devfs / procfs 初始化
    ├─ virtio-9p 初始化并尝试挂载 /root
    └─ 创建内核初始化任务
            │
            ▼
      从 /bin/sh 加载静态 ELF32
            │
            ▼
          Ring 3
```

关键入口：

- `arch/x86/boot.s`：Multiboot 头、初始栈和 `_start`；
- `kernel/kernel.c`：初始化顺序和第一个内核任务；
- `arch/x86/user_entry.s`：首次进入用户态；
- `kernel/elf.c`：读取 ELF program header、映射 `PT_LOAD` 段并返回入口地址；
- `arch/x86/switch.s`：IRQ0、系统调用入口和进程上下文切换。

初始化顺序存在依赖。例如 PMM 必须先于 VMM，文件系统必须先于从 `/bin/sh` 创建用户进程，TSS 必须在 Ring 3 进程运行前准备好内核栈。修改 `kernel_main()` 时应保留这些依赖关系。

## 5. 核心子系统

### 5.1 中断和系统调用

IDT 当前注册了部分 CPU 异常、PIT、键盘 IRQ 和 `int 0x80`：

- `0`：除零；
- `6`：无效指令；
- `13`：通用保护错误；
- `14`：页错误；
- `32`：PIT/IRQ0；
- `33`：键盘/IRQ1；
- `0x80`：用户态系统调用。

系统调用 ABI 定义为：

| 寄存器 | 含义 |
| --- | --- |
| `eax` | 系统调用号；返回时保存结果 |
| `ebx` | 参数 1 |
| `ecx` | 参数 2 |
| `edx` | 参数 3 |
| `esi` | 参数 4 |
| `edi` | 参数 5 |
| `ebp` | 内核入口可接收参数 6，但当前 mlibc 封装不传递它 |

系统调用号目前在两处重复维护：

- `kernel/task/syscall.h` 中的 `SYS_*`；
- `ports/mlibc/sysdeps/muxos/sysdeps.cpp` 中的 `KSYS_*`。

两边必须保持完全一致。系统调用失败时，内核通常返回负 errno；mlibc sysdeps 将其转为正 errno 返回给 mlibc。

### 5.2 物理和虚拟内存

PMM 使用位图管理 4 KiB 物理页：

- `MAX_PAGES` 覆盖 4 GiB 地址空间；
- 当前分配器只从前 256 MiB 恒等映射区域分配可供内核直接解引用的页；
- `pmm_alloc_contig()` 为 legacy virtio ring 和 9P 缓冲区提供连续物理页。

VMM 使用传统 x86 两级页表：

- 内核页目录恒等映射前 64 个 PDE，即 256 MiB；
- 每个用户进程拥有独立页目录，并共享内核 PDE；
- 用户栈顶为 `0x28000000`，当前分配 64 页（256 KiB）；
- 匿名 mmap 从 `0x30000000` 开始，使用每进程 bump allocator；
- 当前 `munmap` 是空操作，不会回收区域，直到地址空间被销毁。

修改内存布局时，要同时核对 `kernel/mm/pmm.h`、`kernel/mm/vmm.c`、`kernel/task/process.c` 和链接脚本。部分注释仍写着旧的 128 MiB/32 PDE，实际代码以 `PMM_IDENTITY_MAPPED_LIMIT = 0x10000000` 和 `KERNEL_PDES = 64` 为准。

### 5.3 进程、调度和信号

进程表位于 `kernel/task/process.c`：

- 最大进程数：64；
- 每进程最大文件描述符数：16；
- PID 单调递增，不等同于进程表下标；
- PIT 以 1000 Hz 产生时钟中断，调度器在 IRQ0 和部分阻塞系统调用路径中切换任务；
- `fork` 深拷贝用户页表；
- `execve` 重新装载静态 ELF，并重建 argv/envp/auxv；
- 支持基础 process group、session 和信号处理，但并非完整 POSIX 实现。

`process_t` 的布局被汇编直接使用。修改结构体时必须同步：

1. `kernel/task/process.h` 中的 `_Static_assert(sizeof(process_t) == 464)`；
2. `arch/x86/switch.s` 中的 `PROCESS_SIZE`；
3. `switch.s` 中所有字段偏移常量。

遗漏任一处都可能造成上下文切换时的静默内存破坏。

### 5.4 文件系统

文件系统分为 VFS 层和后端：

- `kernel/fs/fs.c`：路径解析、全局 open-file table、每进程 fd table、管道及 VFS 操作；
- `kernel/fs/memfs.c`：inode 表、目录项、数据页和启动时根文件系统；
- `kernel/fs/devfs.c`：`/dev/console`、`/dev/tty`、`/dev/null`；
- `kernel/fs/9p_client.c`：9P2000.u 客户端和宿主目录操作；
- `drivers/virtio/virtio_9p.c`：PCI legacy virtio 队列和 9P RPC 传输；
- `kernel/fs/procfs.c`：procfs 的在建骨架。

memfs 的主要限制：

- inode 总数为 128；
- 每个 inode 最多 512 个直接块；
- 块大小为 4096 字节，单文件上限为 2 MiB；
- 没有磁盘持久化、日志、buffer cache、inode 锁和多核同步；
- 当前工作目录保存在全局 `fs_cwd`，并非真正的每进程 cwd。

`kernel/fs/fs_uapi.h` 定义内核侧的 open flag、errno、`stat` 和 `dirent` 布局。mlibc 因命名冲突不能直接包含该文件，所以在 `sysdeps.cpp` 中复制了相关结构；修改 ABI 后必须同步两处。

### 5.5 用户空间和 mlibc

用户程序通过以下链路调用内核：

```text
应用程序
  → mlibc 公共 API
  → ports/mlibc/sysdeps/muxos/sysdeps.cpp
  → ports/mlibc/sysdeps/muxos/syscall.cpp
  → int 0x80
  → arch/x86/switch.s::syscall_stub
  → kernel/task/syscall.c::syscall_handler
```

`flake.nix` 将以下程序作为静态 ELF 嵌入内核镜像：

- `mrsh`，安装为 `/bin/sh`；
- `toybox`，并为启用的 applet 在 `/bin` 创建硬链接；
- `neatvi`，安装为 `/bin/vi`；
- `sfm`，安装为 `/bin/sfm`。

Makefile 生成 `build/programs.S`，通过 `.incbin` 把程序嵌入内核；`fs_init()` 再把这些字节复制到 memfs 的 `/bin`。因此，增加大型静态程序会同时增加内核镜像大小和启动时 memfs 内存占用。

## 6. 常见开发任务

### 6.1 修改内核代码

推荐循环：

```sh
nix --extra-experimental-features 'nix-command flakes' develop
make compile-commands
make
```

如果需要验证完整用户空间，退出开发 shell 后执行：

```sh
nix --extra-experimental-features 'nix-command flakes' build
nix --extra-experimental-features 'nix-command flakes' run
```

内核是 freestanding 环境：

- 不要依赖宿主 libc；
- 使用 `kernel/lib/string.c` 提供的 `kmemcpy`、`kmemset`、`kstrlen` 等函数；
- 避免让编译器生成未提供的运行时符号，如 `memcpy`、`memset` 或 64 位除法辅助函数；
- 中断相关代码不能假设 SSE/FPU 状态已保存，键盘驱动目前显式使用 `-mgeneral-regs-only`；
- 内核中的物理地址通常被当作 32 位整数处理，转换指针时应显式经过 `uintptr_t`。

### 6.2 增加系统调用

建议按以下顺序完成：

1. 在 `kernel/task/syscall.h` 分配新的、未占用的系统调用号；
2. 在 `kernel/task/syscall.c::syscall_handler()` 增加处理分支，并统一返回负 errno；
3. 在 `ports/mlibc/sysdeps/muxos/sysdeps.cpp` 增加相同编号的 `KSYS_*`；
4. 如需新的 mlibc 能力，在 `ports/mlibc/sysdeps/muxos/include/mlibc/sysdeps.hpp` 注册对应 tag，并实现 sysdep；
5. 如果传递共享结构体，在内核和 mlibc 两侧添加静态尺寸/偏移检查；
6. 重新构建 mlibc 和完整 ISO，而不只是增量编译内核；
7. 在用户态编写最小测试，覆盖成功、错误参数和资源耗尽路径。

不要直接把用户指针当作永远有效。当前内核的 copy-in/copy-out 校验仍较弱，新接口至少应检查空指针、长度溢出和用户地址范围。

### 6.3 增加用户程序

仓库采用 Nix 组织用户程序。通常需要：

1. 在 `nix/pkgs/<name>/default.nix` 中用 mlibc 交叉工具链构建静态 i686 程序；
2. 使用 `ports/mlibc/hello.ld` 或等价链接设置；
3. 在 `flake.nix` 的 `programs` 列表中加入 `{ name = "..."; path = "..."; }`；
4. 如为 Toybox applet，优先修改 `nix/pkgs/toybox/default.nix` 的 `toys` 列表；
5. 用 `nix build` 做完整重建，并确认生成文件不超过 memfs 的 2 MiB 单文件上限。

### 6.4 增加设备驱动

驱动一般放在 `drivers/<类别>/`，并由 Makefile 自动纳入构建。一个最小驱动通常包含：

1. 设备探测或固定硬件初始化；
2. I/O port 或 MMIO 访问；
3. 必要的 IRQ 注册和 EOI 处理；
4. 向上层导出的窄接口；
5. 在 `kernel_main()` 中按依赖顺序初始化；
6. 如需用户访问，在 `devsw` 中注册并在 `/dev` 创建 inode。

当前系统面向单 CPU，驱动代码大多没有锁。即便如此，中断上下文与普通内核路径仍可能并发访问数据，必要时应使用短临界区或明确的单生产者/单消费者约束。

## 7. procfs 开发指南

当前 `kernel/fs/procfs.c` 只完成了 `/proc` 目录的创建和 `INODE_PROCFS` 标记，`procfs_read()`、`procfs_readdir()`、`procfs_lookup()` 尚未实现，也没有被 VFS 数据路径调用。因此目前创建 `/proc` 并不等于已有可用的 procfs。

建议先定义最小目标，例如：

```text
/proc/
├── self/
│   └── status
├── <pid>/
│   ├── stat
│   └── status
├── meminfo
└── uptime
```

实现时建议遵循以下设计：

1. **不要再维护独立 inode 表。** `procfs.c` 中当前的静态 `inode_table` 与 memfs 的私有表互不相通，应删除或替换为明确的虚拟节点模型。
2. **定义节点身份。** 可在 `struct inode` 中增加 procfs 节点类型和 PID 等后端字段，或建立可从 inode 映射到 `{kind, pid}` 的受控表。
3. **接入 lookup。** 当前路径遍历调用 `dirlookup()`；遇到 `INODE_PROCFS` 目录时，应改为调用 procfs lookup，而不是读取 memfs 目录项。
4. **接入 read。** `fileread()`/`readi()` 遇到 `INODE_PROCFS` 文件时，应动态生成内容，并正确处理 `off` 和 `count`，保证分段读取与 EOF 行为正确。
5. **接入 getdents。** 当前 `SYS_GETDENTS` 直接按 memfs `struct dirent` 读取 inode 内容；procfs 目录需要后端专用的 readdir，并维护 `file->off` 作为稳定游标。
6. **保证对象生命周期。** 进程可能在目录遍历与文件读取之间退出。生成内容时应再次查找 PID，不要长期保存裸 `process_t *`。
7. **限制写操作。** procfs 默认应为只读；`create`、`unlink`、`rename`、`truncate` 和写入应返回 `EROFS` 或适当 errno。
8. **使用有界格式化。** 内核目前没有完整 `snprintf`，需要提供小型、安全的格式化辅助函数，不能向固定缓冲区无界写入。

可以先实现静态文件 `/proc/meminfo`，验证 open/read/lseek/EOF，再实现 PID 目录。这比一次性同时修改 lookup、readdir 和动态进程节点更容易定位问题。

### procfs 最小验收清单

- `ls /proc` 能稳定列出条目；
- 连续多次、小块读取与一次性读取结果一致；
- `lseek(fd, 0, SEEK_SET)` 后能重新读取；
- 不存在的 PID 返回 `ENOENT`；
- 进程退出后对应目录消失，不发生悬空指针；
- `echo x > /proc/...`、`rm`、`mkdir` 等修改操作返回只读错误；
- inode 或目录游标不会因反复访问而泄漏；
- procfs 未启用或初始化失败时，其他文件系统仍能正常工作。

## 8. 调试

### 8.1 串口日志

控制台输出会镜像到 COM1。调试启动时建议使用：

```sh
qemu-system-i386 \
  -m 512M \
  -cdrom build/muxos.iso \
  -virtfs local,path="$PWD",mount_tag=host0,security_model=none \
  -serial stdio
```

若图形窗口和 `stdio` 发生冲突，可追加 `-display none`，只保留串口。

### 8.2 GDB

让 QEMU 在第一条指令前暂停并开放 GDB 端口：

```sh
qemu-system-i386 \
  -m 512M \
  -cdrom build/muxos.iso \
  -serial stdio \
  -display none \
  -s -S
```

另开终端：

```sh
i686-elf-gdb build/kernel.elf
```

在 GDB 中：

```gdb
target remote :1234
break kernel_main
continue
```

常用断点包括 `kernel_main`、`syscall_handler`、`process_schedule`、`isr14_handler`、`vfs_open_at` 和 `v9p_rpc`。

### 8.3 常见故障定位

| 现象 | 优先检查 |
| --- | --- |
| GRUB 找不到内核 | `build/isodir/boot/kernel.elf` 和生成的 `grub.cfg` |
| 启动后提示无法打开 `/bin/sh` | 是否用 Nix 完整构建，`PROGRAMS` 是否包含 `sh` |
| 页错误后停机 | 串口中的 CR2、EIP、CS 和 error code；是否为用户态 fault |
| 系统调用行为异常 | `SYS_*` 与 `KSYS_*` 编号、参数寄存器和 errno 符号 |
| `ls /root` 失败 | QEMU 是否提供 `mount_tag=host0`、virtio 设备和 9P 日志 |
| 切换进程后随机崩溃 | `process_t` 大小/字段偏移与 `switch.s` 是否同步 |
| 链接出现未定义的 libc 符号 | freestanding C 是否触发编译器生成 `memcpy` 等外部调用 |

## 9. 当前已知限制和开发基线

截至 2026 年 9 月 27 日，当前工作树需要注意：

- `kernel/fs/procfs.c` 的三个非 void 函数没有返回值，编译会产生 `-Wreturn-type` 和未使用参数警告；
- procfs 后端尚未接入 `fileread`、路径 lookup 和 `SYS_GETDENTS`；
- `procfs.c` 声明了自己的静态 inode 表，但实际分配走 memfs 的 `ialloc()`，该表当前无效；
- 直接执行 `make -j2` 能完成编译，但当前链接在 `kernel/task/process.c` 的结构体移动处产生未定义的 `memcpy` 引用；这类移动应改为内核自带的复制函数或确保编译器不会生成宿主 libc 依赖；
- VMM 的部分注释仍描述 128 MiB/32 PDE，当前实现实际为 256 MiB/64 PDE；
- procfs、时间、poll、futex、权限、用户身份、`munmap` 和部分终端语义仍是占位或简化实现；
- 文件系统 cwd 是全局状态，不适合真正并发的多进程目录切换；
- VFS、进程表、设备和文件系统均未设计为 SMP 安全。

这些限制不应被文档或 libc 兼容层掩盖。新增功能时，优先返回准确的 `ENOSYS`/`EROFS`/`EINVAL`，不要用“成功但什么也没做”来模拟尚未实现的语义。

## 10. 提交前检查清单

```sh
make clean
make compile-commands
make -j2
```

对于会影响用户态 ABI、mlibc 或嵌入程序的修改，还应执行：

```sh
nix --extra-experimental-features 'nix-command flakes' build
```

启动后至少检查：

- 启动日志没有新的 panic 或页错误；
- `[FS] selftest PASS`；
- `/bin/sh` 能启动；
- `ls /`、`cat /hello` 和基本文件操作正常；
- `/root` 的 9P 共享目录可读写；
- 修改涉及的系统调用同时覆盖成功和失败路径；
- 新增用户程序能从 `/bin` 执行；
- 不存在新增编译警告，尤其是非 void 函数缺少返回值、指针宽度转换和未声明函数。

如果修改了 ABI，请额外检查：

- `SYS_*` 与 `KSYS_*` 编号；
- `fs_uapi.h` 与 mlibc 中复制的结构布局；
- `process_t` 与汇编偏移；
- ELF、栈和 TLS 仍符合 i386 用户态约定。
