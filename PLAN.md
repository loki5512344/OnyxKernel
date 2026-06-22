# SlipperKernel — Design & Roadmap

## Философия

SlipperKernel — не «ещё один Linux». Это осознанное упрощение под RISC-V embedded.
Выкинуто всё, что не нужно на голом железе без десктопа. Оставлено только то,
что нужно для загрузки → шелла → управления железом.

---

## Ключевые отличия от Linux

### 1. Свой формат бинарников (SPX), не ELF

**Linux**: ELF — сложный, секции, релокации, динамическая линковка, GOT/PLT.

**SlipperOS**: SPX — 344 байта заголовок, 40 байт на сегмент. Никаких релокаций,
GOT, динамики. Что загрузил — то и выполняется.

Польза: загрузка в 10 раз быстрее, код парсера 100 строк, никаких страданий с
динамической линковкой. Для embedded — идеал.

### 2. Нет fork() — только exec()

**Linux**: fork() копирует весь процесс, потом exec(). COW-страницы, MMU-извращения.

**SlipperOS**: `SYS_exec(12)` — загружает SPX и заменяет текущий процесс в той же
адресной ячейке. Всё.

Польза: не нужно управлять COW, не нужно копировать страницы. Для однопоточной
embedded-системы это holywar-плюс.

### 3. Нет mmap/munmap/mprotect

**Linux**: mmap — сложнейший сисколл, флаги, MAP_ANONYMOUS, MAP_SHARED, etc.

**SlipperOS**: `SYS_sbrk(13)` — двигает pointer в предвыделенной 64KB куче.
Выделено сразу, ничего не маппится «на лету».

Польза: никаких page fault'ов при первом доступе к памяти. Детерминированно.

### 4. Один процесс активно, нет preemption

**Linux**: десятки тысяч процессов, CFS-шедулер, приоритеты, cgroups.

**SlipperOS**: round-robin на 4 слота, timer 100Hz. Заснул — передал другому.

Польза: предсказуемость. Никаких tail-latency, priority inversion.
Для real-time embedded — то что надо.

### 5. Нет линуксовой модели драйверов

**Linux**: device tree, platform drivers, driver model, deferred probe, модули.

**SlipperOS**: `fdt_find_uart()` → MMIO адрес → работаем. Всё в ядре, никаких
модулей. Драйвер — это функция, а не ворох структур.

Польза: инициализация железа за микросекунды, а не секунды (как Linux на Duo S).

### 6. Нет контейнеров, namespaces, cgroups, SELinux, AppArmor

**Linux**: горы кода для изоляции.

**SlipperOS**: три кольца защиты (M/S/U) через PMP + Sv39. Всё.

Польза: код изоляции — 100 строк в `boot.S`. Безопасность гарантируется
архитектурой RISC-V, а не программными костылями.

### 7. Тулзы на C, а не на Python

**Linux**: buildroot, Yocto — на Python.

**SlipperOS**: `elf2spx.c`, `mkimage.c` — 150 строк на C, компилируются за
0.1 секунды, без зависимостей. Результат — статический бинарник.

Польза: работает везде, не нужно тащить интерпретатор.

---

## SlipperFS

Собственная файловая система SlipperOS. Read-only в MVP, read-write в планах.

**Характеристики:**
- Блоки по 4096 байт (один блок = 8 секторов virtio-blk)
- Inode: 64 байта, 10 прямых блоков + indirect (в планах)
- Имя файла: до 32 байт
- 32 inode на раздел (в MVP)
- Superblock, inode bitmap, data bitmap, inode table, data blocks

**Формат (блоки 0..N):**
| Блок | Назначение |
|------|-----------|
| 0 | Superblock (magic, version, размеры, смещения) |
| 1 | Inode bitmap (32 бита) |
| 2 | Data bitmap |
| 3 | Inode table (32 inode × 64 байта = 2KB) |
| 4+ | Data blocks |

**Почему своя ФС, а не FAT32/ext4:**
- Минимальный код: ~200 строк на read-only
- Полный контроль над форматом
- Никаких лицензионных ограничений (GPL-3.0 чистая)
- Легко расширять (индирект, журналирование, ext-атрибуты)

---

## Roadmap

### ✅ v0.1 — Базовое ядро (C/ASM rewrite)
- Загрузка: SlipperBoot → kernel.elf (FAT32 диск, MBR partitioned)
- PAM: UART, CLINT timer, PLIC
- MMU: Sv39 identity map (1GB huge pages)
- PMM: bitmap allocator, heap (bump + free list)
- Traps: S-mode handler, user→kernel, kernel→user
- Scheduling: round-robin, 100Hz timer tick
- Syscalls: write/read/exit/yield/open/close/lseek/stat/exec/sbrk
- VFS: SlipperFS + FAT32 read-only
- Drivers: virtio-blk (legacy v1 + modern v2)
- Init: C shell (help, echo, cat, exec, clear, exit)
- Tools: elf2spx.c, mkimage.c (host-side, C)
- Library: Rust core (memcpy, memset, strcmp)

### 🔜 v0.2 — ФС запись + утилиты
- SlipperFS write/create/delete
- Directory listing syscall (`SYS_readdir`)
- Много .spx файлов на диске (init, test, cat, ls)
- Initramfs (init.spx в `.rodata` ядра)

### 🔜 v0.3 — Сеть
- VirtIO net драйвер
- Базовый сетевой стек
- TFTP загрузка

### 🔜 v0.4 — Реальное железо
- Milk-V Duo S / OC2r
- SDHCI в ядре
- GPIO / I2C / SPI

### 🔜 v1.0 — Стабильный релиз
- Документация
