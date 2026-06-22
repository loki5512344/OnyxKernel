# SlipperKernel

<p align="center">
  <img src="https://img.shields.io/badge/platform-RISC--V%2064--bit%20GC-green" alt="RISC-V 64 GC">
  <img src="https://img.shields.io/badge/language-C%20%2B%20ASM-blue" alt="C + ASM">
  <img src="https://img.shields.io/badge/stage-alpha-orange" alt="alpha">
  <img src="https://img.shields.io/badge/license-GPL--3.0-red" alt="GPL-3.0">
</p>

Ядро [SlipperOS](https://github.com/loki5512344/SlipperOS) для архитектуры
RISC-V 64-bit (rv64gc). Запускается загрузчиком
[SlipperBoot](https://github.com/loki5512344/SlipperBoot) и работает в S-mode,
поднимая пользовательский процесс `/bin/init` в U-mode.

Это **первое ядро** — реализован минимально достаточный набор подсистем для
загрузки, инициализации железа и запуска первого пользовательского процесса.
Многое помечено как TODO и будет расширяться поэтапно.

---

## Что уже работает

| Подсистема | Статус | Заметки |
|-----------|--------|---------|
| Boot (M→S переход) | ✅ | PMP, делегирование прерываний, mret в kmain |
| FDT парсер | ✅ | /memory, /model, UART, virtio, sdhci, CLINT, PLIC |
| UART (NS16550A) | ✅ | reg-shift из FDT (QEMU + реальное железо) |
| klog (форматированный вывод) | ✅ | %s %d %u %x %p %c, уровни DBG/INF/WRN/ERR |
| PMM (физическая память) | ✅ | bitmap 4K, alloc/free, alloc_zero |
| VMM (Sv39 paging) | ✅ | 1GB/2MB/4KB huge pages, identity-map kernel |
| Heap (kmalloc/kfree) | ✅ | bump + free-list |
| Trap handler | ✅ | U→S, syscall, page fault, illegal instruction |
| Timer (CLINT, 100Hz) | ✅ | mtimecmp через MMIO, без SBI |
| virtio-blk драйвер | ✅ | legacy v1 + modern v2, polled I/O |
| SlipperFS | ✅ | superblock + inode bitmap + data bitmap + direct blocks |
| FAT32 read-only | ⏳ | stub, возврат -ENOSYS |
| VFS | ✅ | один mount '/', flat namespace |
| SlipperExec (.spx) loader | ✅ | свой формат, 8 сегментов, U-mode mapping |
| Syscalls | ✅ | write, read, exit, yield, getpid, open, close, lseek, stat |
| Process (1 активный) | ✅ | proc_t с kstack, drop_to_user, sscratch swap |
| Scheduler | ⏳ | один процесс; round-robin TODO |
| Initramfs | ⏳ | stub |
| PLIC / внешние прерывания | ⏳ | TODO |
| Real hardware port | ⏳ | QEMU virt only в MVP, DTB-абстракция готова |

---

## Архитектура

### Кольца защиты

SlipperKernel использует модель из трёх колец (как в Linux, но с другим API):

| Кольцо | RISC-V режим | Назначение |
|--------|--------------|-----------|
| 0 (kernel-space) | S-mode | Ядро, драйвера, paging, PMM |
| 1 (root-space) | S-mode, отдельный PMP region | Системные сервисы, package manager (TODO) |
| 2 (user-space) | U-mode | Пользовательские программы |

В MVP кольцо 1 не используется — все драйвера в кольце 0. Каркас PMP
размечен, но без отдельного root-space региона (см. TODO в `asm/boot.S`).

### Memory map (QEMU virt, 256M)

```
0x00000000 .. 0x0FFFFFFF   MMIO (UART, virtio, CLINT, PLIC)
0x02000000                 CLINT
0x0C000000                 PLIC
0x10000000                 NS16550A UART
0x10001000                 virtio,mmio #1
0x10008000                 virtio,mmio #2 (опционально)
0x80000000                 DRAM base, SlipperBoot (~9.5K)
0x80200000                 SlipperKernel entry
0x80200000 + kernel_size   heap (4MB)
...                        PMM bitmap + free pages
0x90000000                 (конец 256M QEMU DRAM)
```

### Boot flow

1. **SlipperBoot** (M-mode, через `-bios bootloader.bin`) читает `kernel.elf`
   с FAT32/ext4 раздела virtio-blk, парсит ELF64, прыгает в `_start` с
   `a0=hartid`, `a1=fdt_addr`.
2. **`_start`** (asm/boot.S, M-mode):
   - Park all harts except hart 0.
   - Zero BSS, setup stack.
   - Configure PMP (kernel + MMIO regions).
   - Delegate page faults, ecall-from-U, timer/external interrupts to S-mode.
   - Set mstatus.MPP = S, mepc = kmain.
   - `mret` into kmain (S-mode, satp=BARE = physical addressing).
3. **`kmain`** (src/main.c, S-mode):
   - fdt_init, uart_init, banner.
   - pmm_init (DRAM from FDT).
   - vmm_init (Sv39, identity-map kernel + MMIO + DRAM, install satp).
   - heap_init, trap_init, timer_init.
   - virtio_blk_init (probe all virtio,mmio nodes from FDT).
   - vfs_init + vfs_mount_root (try SlipperFS first, then FAT32).
   - vfs_open("/bin/init"), kmalloc, vfs_read.
   - spx_load (parse .spx, alloc user root, map segments + stack, mirror in kernel root).
   - proc_init + proc_create_user (init pid=1).
   - `csr_set sstatus, SIE` (enable S-mode interrupts for timer).
   - `proc_enter_user(1)` → `drop_to_user` (asm) → sret into U-mode.
4. **`/bin/init`** (test/init.S, U-mode):
   - SYS_write(1, "Hello from SlipperOS /bin/init!", 33).
   - SYS_exit(0).
5. proc_exit → khalt.

---

## SlipperExec (.spx) формат

Собственный формат пользовательских бинарников. НЕ POSIX, НЕ ELF.

```c
struct spx_header {
    u32 magic;          // 'SPX1' = 0x31585053
    u32 version;        // 1
    u64 entry;          // virtual entry address
    u32 nsegs;
    u32 flags;          // bit1 = ring1 binary (root-space, TODO)
    spx_segment_t segs[8];
};
struct spx_segment {
    u64 vaddr;
    u64 filesz;
    u64 memsz;
    u32 offset;         // into .spx file
    u32 flags;          // VMM_R | VMM_W | VMM_X
    u32 align;
    u32 reserved;
};
```

Заголовок 344 байта, затем данные сегментов подряд. Нет релокаций, нет
динамической линковки.

Скрипт `scripts/elf2spx.py` конвертирует ELF64 RISC-V в .spx, вытаскивая все
PT_LOAD сегменты.

---

## SlipperFS формат

Простая файловая система, разработанная под SlipperKernel. Не POSIX-совместимая.

```
block 0   : superblock  (spfs_super_t, 64 байта)
block 1   : inode bitmap (1 бит на inode)
block 2   : data bitmap  (1 бит на data block)
block 3+  : inode table  (inodes 64 байта каждая, 64 на блок)
block N+  : data blocks
```

Inode: 64 байта, 10 direct blocks + 1 indirect (TODO).

Корневой каталог — массив `spfs_dirent_t { char name[32]; u32 inode; }` в
первом блоке данных root inode.

Скрипт `scripts/mkimage.py` собирает образ диска с `/bin/init` из .spx файла.

---

## Сборка

### Зависимости

- `riscv64-elf-gcc` или `riscv64-unknown-elf-gcc` (bare-metal кросс-компилятор)
- `riscv64-elf-objcopy` (обычно в комплекте)
- `python3` (для elf2spx.py и mkimage.py)
- `qemu-system-riscv64` (для запуска)
- SlipperBoot (для загрузчика)

### Установка тулчейна (Ubuntu/Debian)

```bash
sudo apt install gcc-riscv64-unknown-elf qemu-system-riscv
# или
sudo apt install gcc-riscv64-linux-gnu qemu-system-riscv
```

### Сборка SlipperBoot

```bash
git clone https://github.com/loki5512344/SlipperBoot.git
cd SlipperBoot
make CROSS=riscv64-elf
# получите bootloader.bin
```

### Сборка SlipperKernel

```bash
git clone https://github.com/loki5512344/SlipperKernel.git
cd SlipperKernel
make CROSS=riscv64-elf
# получите build/kernel.elf, build/init.spx, build/disk.img
```

### Запуск в QEMU

```bash
SLIPPERBOOT_DIR=/path/to/SlipperBoot bash scripts/run_qemu.sh
```

Или вручную:

```bash
qemu-system-riscv64 \
    -M virt -m 256M \
    -bios /path/to/SlipperBoot/bootloader.bin \
    -drive file=build/disk.img,format=raw,if=none,id=drv0 \
    -device virtio-blk-device,drive=drv0 \
    -nographic -serial mon:stdio -no-reboot
```

Ожидаемый вывод:

```
SlipperBoot v0.4 [riscv-virtio,qemu]
SlipperBoot boot menu
--------------------
  0: VirtIO @ 0x0000000010008000
--------------------
loading kernel.elf
jumping to kernel

  ___ _ _                  _
 / __(_) |_ _____ __ _____| |__
 \__ \ | \ V / -_) V /___| / /_
 |___/_|_|\_/\___|\_/    |_\__/
  SlipperKernel v0.1 — RISC-V 64 GC

[INF] kmain: hartid=0 fdt=0x...
[INF] platform: riscv-virtio,qemu
[INF] PMM: dram 0x80000000 + 0x10000000
[INF] PMM: managed 0x..., pages=... free=...
[INF] vmm: Sv39 on, kernel root @0x...
[INF] heap: 0x... + 0x400000
[INF] trap: stvec=0x...
[INF] timer: CLINT @0x2000000, tick=... ns
[INF] fdt: 1 virtio,mmio node(s)
[INF] virtio-blk[0] @0x10008000 v2 (modern)
[INF] slipperfs: mounted v1, ... blocks, ... inodes
[INF] vfs: root mounted (SlipperFS)
[INF] kmain: /bin/init size=...
[INF] spx: seg 0 va=0x10000 ...
[INF] spx: entry=0x10000 root=0x... ustack=0x...
[INF] proc: entering user pid=1 entry=0x10000
Hello from SlipperOS /bin/init!
[INF] proc: pid 1 exited with code 0
[INF] proc: no more processes, halting
```

---

## Структура проекта

```
SlipperKernel/
├── Makefile               # кросс-сборка, цели kernel.elf / init.spx / disk.img / run
├── linker.ld              # 0x80200000, .text/.rodata/.data/.bss/.stack
├── README.md              # этот файл
├── include/               # заголовки
│   ├── types.h            # u8..u64, SL_ERR_*, MACROS
│   ├── riscv.h            # CSR / PMP / PTE / CLINT / PLIC defines
│   ├── klog.h             # kinf/kerr/kwrn/kdbg/kpanic
│   ├── uart.h             # NS16550A
│   ├── fdt.h              # парсер device tree
│   ├── pmm.h              # физическая память
│   ├── vmm.h              # Sv39 paging
│   ├── heap.h             # kmalloc/kfree
│   ├── trap.h             # trap_frame_t
│   ├── timer.h            # CLINT timer
│   ├── virtio.h           # virtio-blk MMIO
│   ├── slipperfs.h        # SlipperFS on-disk format
│   ├── fat32.h            # FAT32 stub
│   ├── vfs.h              # VFS layer
│   ├── spx.h              # SlipperExec format
│   ├── proc.h             # process descriptor
│   └── syscall.h          # syscall numbers
├── asm/
│   ├── boot.S             # _start, M→S переход
│   └── trap.S             # trap_entry, drop_to_user
├── src/
│   ├── main.c             # kmain
│   ├── klog.c             # форматированный вывод
│   ├── fdt.c              # FDT parser
│   ├── pmm.c              # bitmap allocator
│   ├── vmm.c              # Sv39 huge pages
│   ├── heap.c             # free-list allocator
│   ├── trap.c             # trap dispatch
│   ├── timer.c            # CLINT 100Hz
│   ├── syscall.c          # syscall handler
│   ├── vfs.c              # VFS
│   ├── spx.c              # .spx loader
│   └── proc.c             # process management
├── drivers/
│   ├── uart.c             # NS16550A impl
│   └── virtio.c           # virtio-blk impl
├── fs/
│   ├── slipperfs.c        # SlipperFS impl
│   └── fat32.c            # FAT32 stub
├── test/
│   ├── init.S             # /bin/init в ассемблере
│   └── init.ld            # 0x10000
└── scripts/
    ├── elf2spx.py         # ELF → .spx конвертер
    ├── mkimage.py         # собирает SlipperFS образ
    └── run_qemu.sh        # запуск в QEMU
```

---

## Roadmap

### v0.2 (следующий шаг)
- [ ] Реальный FAT32 read-only драйвер
- [ ] Initramfs поддержка (через модули SlipperBoot)
- [ ] PLIC + внешние прерывания (для virtio-blk IRQ mode)
- [ ] Scheduler: round-robin с переключением контекста
- [ ] Indirect blocks в SlipperFS (для больших файлов)
- [ ] Pipe syscall (для shell)

### v0.3
- [ ] root-space (кольцо 1) через PMP region 1
- [ ] Системные сервисы (systemd-like, но своё)
- [ ] brk / mmap syscall (полноценный user heap)
- [ ] fork / exec / wait (многозадачность)
- [ ] SIGNALS (свой API, не POSIX)

### v0.4+
- [ ] SMP (мульти-hart)
- [ ] Сеть (virtio-net)
- [ ] USB (xHCI)
- [ ] GPU (virtio-gpu / простой framebuffer)
- [ ] Real hardware port (SiFive HiFive Unmatched)

---

## Лицензия

GPL-3.0-or-later. См. [LICENSE](LICENSE) (TODO: добавить файл).

## Контрибьюторы

- loki5512344 — основатель, разработчик SlipperBoot/SlipperOS
- SlipperKernel v0.1 — initial scaffold
