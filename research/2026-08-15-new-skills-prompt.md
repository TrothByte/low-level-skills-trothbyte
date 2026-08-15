# PROMPT: создание новых скиллов TrothByte по итогам трёх ресёрчей

> Этот файл — готовый промпт для субагента/Agent Manager. Скопируй его целиком в
> новую сессию (или разбей по батчам согласно §7). Всё, что нужно агенту, —
> репозиторий TrothByte/low-level-skills-trothbyte и этот промпт.

---

## МИССИЯ

Создай **новые, максимально качественные скиллы** для репозитория
 по итогам трёх ресёрч-файлов в `research/`:

1. `research/2026-08-15-agent-failures-survey.md` — ~55 задокументированных отказов
   ИИ-агентов в low-level коде (CONCUR, RustEvo², крейт-галлюцинации, Ghostty,
   ISO-Bench, LLVM-Bench, крипто-Rust и др.).
2. `research/2026-08-15-external-repos-audit.md` — аудит 18+ внешних skill-репозиториев:
   подтверждён пробел Zig, новые домены (virtualization, risc-v/CHERI, hpc, hdl,
   mobile-android-re) и доработки (kernel, sanitizers, auto-re, bootloader, networking).
3. `research/2026-08-15-asm-agent-failures-survey.md` — галлюцинации ассемблера:
   выдуманные mnemonics, AT&T-инверсия, NASM-классы, Thumb-2-ошибки, декомпиляция
   plausible-but-wrong, LLM-as-compiler.

**Жёсткие требования:**
- Скиллы должны быть **новыми** — ни один не дублирует существующие 60 скиллов
  (список ниже в §4) и не дублирует друг друга.
- Каждый скилл — **реально полезный**: решает задокументированный класс ошибок,
  а не «тему ради темы».
- Максимум пользы на единицу effort: приоритет source-backed-возможности
  (доступный тулчейн) и подтверждённости проблемы несколькими источниками.

---

## 1. ОБЯЗАТЕЛЬНЫЙ ПРЕДВАРИТЕЛЬНЫЙ ШАГ (resume protocol)

Прежде чем что-либо делать:
1. Прочитай `AGENTS.md` (инженерные правила + resume protocol + token economy).
2. Прочитай `roadmap/progress.yaml` и `WORKLOG.md`.
3. Прочитай все три файла из `research/` целиком — это единственный источник
   фактического материала. Не выдумывай новых фактов.
4. Прочитай `registry/skills.yaml`, `registry/cross-links.yaml`,
   `registry/evals.yaml`, `registry/claims.yaml`, `registry/sources.yaml`.

## 2. СХЕМА СКИЛЛА (строго, по образцу существующих)

Каждый скилл = директория `skills/<domain>/<skill-id>/` с файлами:

```
SKILL.md          операционный, компактный (~150-400 строк): When to Use / When NOT
                  to Use, что агент делает не так (конкретные классы ошибок с
                  примерами), правильный процесс рассуждения (позитивные паттерны),
                  гейты верификации, ссылки на references/ и evals/
references/       глубокие знания по требованию (progressive disclosure), НЕ монолит
examples/
  good/           рабочие примеры, скомпилированные/прогнанные на реальном тулчейне
  bad/            контрпримеры с пометкой «intentionally vulnerable/incorrect»
evals/README.md   evals по 4 категориям (см. §5) + verified facts
```

Правила (из AGENTS.md): SKILL.md компактный; детали в references; детерминированные
операции — в скрипты; помечать KNOWN / INFERRED / UNVERIFIED; каждый нормативный
claim → ссылка на первичный источник.

## 3. ПЛАНКА КАЧЕСТВА (TrothByte standard)

1. **Source-backed где возможно**: примеры реально компилируются/исполняются на
   доступном тулчейне (gcc/clang/rustc/zig/nasm/qemu/python); где тулчейн недоступен —
   честно `researched` с точными командами.
2. **Claim → source**: каждый нормативный факт трассируется: claim → источник
   (стандарт/документация/CVE/статья) → раздел → скилл. Регистрируй в
   `registry/claims.yaml` и `registry/sources.yaml`.
3. **Progressive disclosure**: SKILL.md компактный, references подгружаются по требованию.
4. **Позитивные паттерны**: скилл учит правильному процессу рассуждения, не только
   «не делай так»; списки анти-паттернов — с обоснованием.
5. **Evals по всем 4 категориям**: synthetic (целевые задания), false-positive
   (скилл не должен срабатывать на корректном коде), historical (реальные инциденты
   с известным root cause), adversarial (попытки обмануть/сломать гейты).
6. **Регистрация до создания**: скилл получает запись в `registry/skills.yaml`
   (id, domain, type, priority, tier, stability) ДО создания SKILL.md.
7. **Калибровка**: quantitative-данные из ресёрчей (проценты успеха, FP-rate) —
   в evals/README.md как verified facts.

## 4. СУЩЕСТВУЮЩИЕ 60 СКИЛЛОВ (запрещено дублировать)

c: c-undefined-behavior, c-integer-promotion-and-conversion, c-string-and-buffer-safety,
c-errno-and-syscall-returns, c-signal-handler-safety · cpp: raii-descriptor-types-api-design,
cpp-object-lifecycle, cpp-move-semantics · rust: rust-unsafe-reasoning, rust-ffi-boundary,
rust-panic-safety · concurrency: memory-ordering-reasoning, atomics-c11-cpp11-rust,
concurrency-deadlock-and-lock-ordering, concurrency-condvar-and-spurious-wakeup ·
compiler: compiler-ub-assumptions · llvm: llvm-ir-reading, llvm-pass-writing ·
assembly: asm-x86-64-registers-and-addressing, asm-calling-conventions,
asm-inline-asm-constraints, asm-signed-unsigned-branches, asm-optimizer-artifacts ·
abi: abi-layout-reasoning · ffi: ffi-boundary-cross-language · elf: elf-linker-loader-debugger,
elf-layout-and-relocations, elf-dynamic-linking-got-plt · dwarf: dwarf-debug-info ·
kernel: kernel-rcu-memory-barriers, kernel-uaccess-safety, kernel-atomic-context ·
networking: ebpf-verifier-reasoning · embedded: embedded-mpu-trustzone,
embedded-volatile-and-memory-ordering, rtos-concurrency-and-isr,
embedded-interrupt-and-nested, embedded-linker-script · bootloader: bootloader-stages ·
qemu: qemu-system-setup · binary-analysis: binary-analysis-type-recovery ·
reverse-engineering: auto-re-protocols-beyond-can, go-rust-re · performance:
cache-and-numa-optimization, performance-measurement-discipline · simd:
simd-vectorization-cross-layer, vectorization-reasoning · gpu: ptx-assembly,
gpu-memory-model-coherence · sanitizers: sanitizer-agent-ci-loop,
sanitizer-report-reading · _meta: meta-routing, meta-evidence, meta-verification,
meta-assumptions, meta-rationalizations, meta-completion, safe-low-level-from-scratch,
zeroize-constant-time, wasm-runtime-from-scratch

**Анти-дубль:** прежде чем создавать скилл, сверься с этим списком и
`registry/cross-links.yaml`. Если новый скилл пересекается более чем на ~30% с
существующим — не создавай, а предложи дополнение существующего (отдельной строкой
в отчёте).

## 5. СПИСОК НОВЫХ СКИЛЛОВ (создай ВСЕ, каждый с указанными источниками и гейтами)

Формат для каждого: `ID — название — [проблема из ресёрча] [первичные источники]
[тулчейн верификации] [категории evals]`.

### A. Assembly / ассемблер (домен assembly)

1. **asm-verification-hallucination-gate** — [галлюцинации mnemonics/opcodes:
   CDC-COMPASS-выдумки, `movl`-инверсия, `AX=8bit`, `[esp+4]` vs `[esp]`, `imul
   eax,eax,38`, байт-уровневая слепота; ASM-1..21 из asm-survey] [Intel SDM, ARM ARM,
   NASM Manual] [nasm/gas/as + objdump + qemu -d exec: ассемблируй → дизассемблируй →
   сравнивай байты; проверка существования инструкции по справочнику] [synthetic +
   historical (HerraduraKEx PR#33, BBoeOS PR#584) + FP (box64 #4214)].

2. **asm-syntax-dialects-nasm-gas-att** — [4 класса NASM-ошибок: case-sensitivity,
   `mov rax,buf` vs `[buf]`, size hints, `default rel`; AT&T vs Intel операнды;
   `$` в NASM 3.x директивах] [NASM Manual, GNU as docs, System V ABI] [nasm/gas +
   gcc -S] [synthetic + FP (валидный код не должен флагаться)].

3. **asm-arm-thumb-2-encoding** — [Thumb-2 `cbz/cbnz` только r0–r7 (HerraduraKEx
   PR#33), 16/32-bit кодирование, condition-коды, ветвления] [ARM ARM, ARM
   Architecture Reference Manual Thumb-2] [clang -target arm + llvm-mc + qemu-arm]
   [synthetic + historical].

4. **asm-aarch64-neon-simd-safety** — [NEON-SIMD пропуск per-lane overflow-guard
   (Lemire: переполнение каждые 255 итераций); битмаска-арифметика] [ARM NEON
   Intrinsics Reference, Armv8 ISA] [clang -target aarch64 + qemu-aarch64 +
   тест на краевых инпутах] [synthetic + adversarial (переполнение счётчиков)].

5. **asm-risc-v-registers-and-calling-conventions** — [RISC-V рекурсия: неинициализированный
   `s0`, кадр 4 вместо 8 байт; callee-saved/leaf-функции] [RISC-V ISA Manual vol.1,
   RISC-V psABI] [clang/riscv64-gcc + qemu-riscv64] [synthetic + historical].

### B. Binary analysis / Reverse engineering (домены binary-analysis, reverse-engineering)

6. **binary-disassembly-decompilation-fidelity** — [декомпиляция plausible-but-wrong:
   DeGPT CFR 37%, SCDBench 7%, Meta-LLM-Compiler exact 14%, cliff на ~200
   инструкциях, «compile@k ≠ pass@k»; DEC-1..13 из asm-survey] [Meta LLM Compiler
   (2407.02524), LLM4Decompile (2403.05286), FidelityGPT (2510.19615)] [gcc/objdump,
   re-executability-тесты: декомпилируй → перекомпилируй → прогони тесты; byte-round-trip]
   [synthetic + FP (не доверять «чистому» дизассемблеру)].

7. **reverse-engineering-shellcode-analysis** — [GPT-3 разбор shellcode: неверные
   syscalls, выдуманная инструкция, не извлечён IP/порт из констант (ArchCloudLabs)]
   [Linux syscall table, Intel SDM] [objdump/capstone + strace/emulation] [synthetic +
   historical].

8. **reverse-engineering-can-signal-extraction** — [CAN/automotive: DBC-формат,
   cantools, sawtooth bit numbering, scale/offset, SWEEP/HOLDS, parked-vs-moving гейт
   (CSS-Electronics 4/5)] [DBC spec, cantools docs, ISO 11898-1] [cantools + python +
   verify.py-паттерн] [synthetic + FP].

9. **reverse-engineering-ghidra-agent-automation** — [агент↔Ghidra loop: daemon/RPC,
   function triage → annotate → type recovery → diff; уверенные, но неверные выводы
   (Quesma «Centipede» vs River Raid)] [PyGhidra, Ghidra API] [ghidra headless +
   pytest] [synthetic + FP].

10. **binary-memory-leak-vm-allocator-diagnosis** — [VM-утечка: mmap-пул Ghostty,
    пропущенный munmap, 37–130 GB; «агент — триггер, не причина»] [mitchellh.com
    ghostty-memory-leak-fix, Linux mm/mmap docs] [valgrind/massif + /proc/maps + RSS
    трекинг] [historical + FP (ложная атрибуция)].

### C. Concurrency (домен concurrency)

11. **concurrency-actual-parallelism-detection** — [«фейковый параллелизм»: thread-safe
    примитивы при однопоточном исполнении (CONCUR, категория ST); zsh `jobs`-в-command-
    substitution лимит → kernel panic (codex#37653)] [CONCUR (2603.03683), codex#37653,
    POSIX threads] [pthread/std::thread + sched_getaffinity + /proc/<pid>/stat +
    strace clone] [synthetic + historical].

### D. Rust (домен rust)

12. **rust-api-evolution-and-drift** — [API-эволюция: 65.8% стабилизированные vs 38.0%
    поведенческие; 56.1% до cutoff vs 32.5% после; RAG +13.5% (RustEvo²); bevy#23867;
    stale `format!`] [RustEvo² (2503.16922), rustc #[deprecated], cargo doc] [cargo
    check/build против зафиксированной версии, cargo-semver-checks] [synthetic +
    historical].

13. **rust-dependency-supply-chain** — [крейт-галлюцинации: несуществующие крейты,
    похожие на реальные (2606.08444); 5.2%/21.7% пакетных галлюцинаций (2406.10279)]
    [crates.io API, cargo search, cargo audit/deny] [cargo search + crates.io API +
    typosquat-сравнение имён] [synthetic + adversarial].

14. **rust-crypto-primitives-safety** — [крипто-Rust: 23.3% компилируемость, 57%
    уязвимых из скомпилированных, nonce reuse, API-галлюцинации, CoT 5× хуже
    (2604.27001)] [NIST SP 800-38D (GCM), ChaCha20-Poly1305 RFC 8439, rustcrypto/ring]
    [cargo test с answer vectors, clippy, rule-based анализ] [synthetic + FP +
    adversarial (nonce reuse)].

### E. Embedded (домен embedded)

15. **embedded-hw-register-datasheet-verification** — [галлюцинации регистров:
    ST7789 MADCTL, «красивый код, который не работает», Cursor-STM32 (несуществующие
    регистры/HAL-функции), Zephyr-регистры] [datasheet'ы (конкретные для примеров),
    ARM CMSIS] [компиляция + чтение datasheet + verilog/C-stub референс] [synthetic +
    FP].

16. **embedded-device-tree-and-kconfig** — [Zephyr: галлюцинации `compatible`-строк,
    Kconfig-символов, node-address коллизии (reversetobuild)] [devicetree spec,
    Zephyr docs] [west build + dtc + zephyr toolchain] [synthetic + FP].

### F. Kernel (домен kernel)

17. **kernel-scheduler-mm-vfs-internals** — [CFS→EEVDF (6.6→6.12), vruntime/lag,
    buddy/SLUB, vmalloc vs kmalloc, VFS dcache/inode] [docs.kernel.org (scheduler/mm/
    filesystems), kernel source] [Linux в QEMU + /proc/* + ftrace/perf] [synthetic +
    historical].

18. **kernel-debugging-ftrace-kprobes-kdump** — [kgdb/kdb, ftrace, kprobes, dyndbg,
    kdump/crash] [docs.kernel.org/trace, crash docs] [QEMU + kgdb/ftrace] [synthetic].

19. **kernel-container-internals** — [namespaces, cgroups v2, overlayfs, OCI/runc,
    seccomp, caps] [namespaces(7), cgroup v2 docs, OCI runtime spec] [Linux + unshare/
    nsenter + systemd-run] [synthetic + historical].

### G. Networking (домен networking)

20. **networking-hardware-rdma-nic-offload** — [RDMA-семантика: QP/MR/atomic ops,
    transport vs link, NIC offload/DPDK/eSwitch; документарно (нет оборудования)]
    [InfiniBand/RoCE verb docs, NVIDIA DOCA docs] [libibverbs + perftest если доступно,
    иначе docs + QEMU] [synthetic + FP].

### H. Sanitizers (домен sanitizers)

21. **fuzzing-harness-evidence-gate** — [Proof Standard: нет CVE без воспроизводимого
    sanitizer-отчёта + минимизированного инпута + достижимого пути (fuzz-skill 0xazanul)]
    [AFL++/libFuzzer docs, OSS-Fuzz] [AFL++/libFuzzer + ASan/UBSan/MSan + crash
    minimization] [synthetic + FP (не заявлять без доказательств)].

### I. GPU (домен gpu)

22. **gpu-kernel-verification-beyond-oracle** — [иллюзия корректности: fixed-shape
    allclose-оракулы сертифицируют бажные ядра; fuzz+fp64-референс ловит 9/9
    (2606.20128); ядра «проходят review, сегфолтят под нагрузкой» (ISO-Bench)]
    [ISO-Bench (2602.19594), Correctness Illusion (2606.20128), CUDA/Triton docs]
    [nvcc/hipcc + сгенерированный fuzz-раннер + fp64-референс] [synthetic +
    adversarial (невиданные shapes)].

23. **gpu-communication-primitives** — [GPU-коммуникации: p2p/collectives/expert-
    parallel, 30.7% лучшая модель (CommBench)] [CommBench (2608.04450), NCCL docs]
    [NCCL + multi-GPU если доступно, иначе docs] [synthetic].

### J. Zig — НОВЫЙ ДОМЕН (пробел подтверждён 5 репо; тулчейн бесплатный) — домен zig

24. **zig-comptime-metaprogramming** — [comptime, @typeInfo/@Type/@TypeOf, comptime
    for vs inline for, пределы вычислений] [ziglang.org langref «Comptime»] [zig
    build test] [synthetic].
25. **zig-allocators-and-memory-management** — [std.heap: Arena, GPA/Debug, FixedBuffer,
    MemoryPool; дизайн аллокаторов] [Zig std source, langref] [zig test с тест-
    аллокатором] [synthetic + adversarial (утечки/двойное освобождение)].
26. **zig-version-migration** — [0.15→0.16→0.17 breaking changes: std.Io/Writergate,
    Juicy Main, @Type split; методология пининга версий (zigcc VERSIONING.md — идея)]
    [zig release notes per version, langref] [zigup + zig build против пинов] [synthetic
    + historical].
27. **zig-inline-asm-and-abi** — [asm-блоки, типизированные clobber'ы, calling
    conventions, @export/@extern] [langref «Inline Assembly», System V ABI] [zig +
    objdump] [synthetic].
28. **zig-ffi-c-interop** — [@cImport/translate-c, extern struct, C-ABI типы, std.c]
    [langref, Zig std.c source] [zig + cc + linker] [synthetic].
29. **zig-build-system-and-packages** — [build.zig/build.zig.zon, пакеты, интеграция C]
    [build-system guide, langref] [zig build] [synthetic].
30. **zig-cross-compilation-targets** — [target triples, zig cc как кросс-компилятор,
    std.Target] [Zig docs, std.Target source] [zig build -Dtarget + qemu] [synthetic].
31. **zig-simd-vector-intrinsics** — [@Vector, std.simd, LLVM-intrinsic interop]
    [langref, Zig std.simd source] [zig test + qemu-векторные тесты] [synthetic +
    adversarial (overflow-guard, как ASM-4)].
32. **zig-error-model-and-defers** — [error unions, defer/errdefer, optional, UB-правила]
    [langref, Zig std source] [zig test] [synthetic].
33. **zig-concurrency-and-io-events** — [std.Thread, атомики, std.Io.Evented/io_uring]
    [Zig std source, io_uring docs] [zig test + qemu] [synthetic + FP].
34. **zig-fuzzer-and-testing** — [встроенный фаззер testOne(*std.testing.Smith),
    корпус, тест-аллокатор] [std.testing source, Zig docs] [zig test + fuzz-режим]
    [synthetic + adversarial].

### K. Virtualization — НОВЫЙ ДОМЕН (домен virtualization)

35. **hypervisor-vmx-svm-internals** — [VT-x VMCS/VMXON/VMLAUNCH, SVM VMCB/VMRUN,
    EPT/NPT, APICv/Posted interrupts, VMEXIT reason-коды] [Intel SDM Vol.3C, AMD APM
    Vol.2] [KVM ioctl-демо /dev/kvm в QEMU (TCG на Windows-хосте)] [synthetic +
    historical (L1TF/MDS/Spectre-v2 mitigations) + adversarial (VMEXIT-storm)].

### L. RISC-V — НОВЫЙ ДОМЕН (домен riscv)

36. **riscv-isa-and-rvv-intrinsics** — [VL/AVL strip-mining, LMUL/SEW, tail/mask
    policies, strided/segmented loads] [RISC-V V spec 1.0, RVV intrinsics docs]
    [clang RVV + qemu-riscv64] [synthetic + adversarial (невиданные shapes)].
37. **riscv-cheri-capability-safety** — [capability-указатели: bounds/tags, purecap/
    hybrid ABI, Morello/riscv64, CheriBSD] [CHERI spec, CheriBSD docs] [QEMU-CHERI +
    cheribuild] [synthetic + FP].

### M. HPC — НОВЫЙ ДОМЕН (домен hpc)

38. **hpc-mpi-programming** — [MPI: p2p/collectives/non-blocking/comm_split, гибрид с
    OpenMP, MPI-IO, mpirun/Slurm] [MPI-4.1 standard] [mpicc + mpirun на localhost
    (MS-MPI/MPICH)] [synthetic + adversarial (deadlock/ordering)].
39. **hpc-openmp-parallel-programming** — [schedules, reductions, target offload]
    [OpenMP 5.x spec] [gcc -fopenmp] [synthetic + FP (race-детекция)].
40. **hpc-rdma-verbs** — [libibverbs QP/CQ/MR, RC/UC/UD, RoCE vs IB, perftest]
    [InfiniBand verb docs, rdma-core] [документарно (нет оборудования)] [synthetic].

### N. HDL/FPGA — НОВЫЙ ДОМЕН (домен hdl)

41. **hdl-cdc-audit** — [CDC: классификация, «constraint не чинит bug», classify-before-
    fix] [CDC-методология (Cliff Cummings), Xilinx/Intel docs] [Verilator + iverilog
    (OSS вместо Vivado)] [synthetic + FP].
42. **hdl-timing-closure** — [timing closure, QoR, «не делай число зелёным, скрывая
    violation»] [Xilinx UG/Intel timing docs] [yosys + nextpnr] [synthetic].
43. **hdl-constraints-authoring** — [SDC-констрейнты, lint-triage] [SDC standard]
    [yosys/nextpnr] [synthetic + FP].

### O. Mobile Android RE — НОВЫЙ ДОМЕН (домен mobile)

44. **android-re-dex-smali-format** — [DEX-формат/инструкции, smali, ART internals,
    R8/ProGuard + Kotlin-metadata deobfuscation] [DEX spec (dalvik-bytecode), ART docs]
    [jadx/apktool/dex2jar + baksmali] [synthetic + historical].
45. **android-re-native-jni-analysis** — [нативные .so/JNI, fingerprint-first, Frida]
    [JNI spec, Android NDK docs] [frida + objdump (ARM/ARM64)] [synthetic + FP].

### P. UEFI / firmware (домен bootloader)

46. **bootloader-uefi-firmware** — [UEFI/PI spec-boundary, edk2, HII/VFR, ACPI/SMBIOS,
    Secure Boot] [UEFI spec, edk2 docs] [QEMU + OVMF + serial-log debugging] [synthetic
    + FP].

### Q. _meta (мета-слой)

47. **meta-verification-harness-validity** — [«прошедший» harness, который не тестирует
    цель (безусловный repaint маскирует баг — claude-code#82057; asm, который никогда
    не исполнялся — qemu -d exec, MintVID); «проверь проверку»] [методология
    trailofbits/wshobson (ablation-Δ, coverage-gate, --self-test) — как идея] [grep
    "прохождения" harness'ов на связь с целью; qemu -d exec для ассемблера] [FP].

### R. Build systems & toolchains — НОВЫЙ ДОМЕН (домен build-systems)

48. **build-system-cmake-diagnostics** — [«починка» CMake dependency-declaration:
   галлюцинации версий/зависимостей вместо диагноза target-графа (HN bsder, 2025);
   escape-loop: переписывание CMakeLists «home-cooked» логикой вместо toolchain env +
   `find_package`, мок-бенчмарк объявлен «успехом» (TensorRT, minimaxir, 2025)]
   [CMake docs, pkg-config docs] [cmake + ninja + проверка target-графа] [synthetic + FP].

49. **build-toolchain-version-drift** — [компилятор/glibc/libstdc++/ABI-версии, `-std=`;
   NTTP-ошибка «обвинён» GCC (r/cpp_questions); устаревший TensorRT-API; `-O0/-O2/-O3`
   дают идентичные бинарники (CCC)] [GCC/Clang docs, glibc ABI] [gcc/clang + --version/
   --print-file-name] [synthetic + FP].

50. **build-linker-error-diagnostics** — [undefined refs, static/shared, LTO/ODR,
   symbol interposition, `--as-needed`; каскад 40,784 undefined refs при линковке ядра
   Linux (`__jump_table`, `__ksymtab` — CCC, 2026)] [GNU ld/lld docs] [ld/lld + nm/readelf]
   [synthetic + historical].

51. **build-process-signal-and-state-safety** — [SIGTERM vs SIGINT: ninja/make умирают на
   `fwrite` → порча `.ninja_deps`, полные пересборки (claude-code#49233); sandbox молча
   no-op'ит cmake/ninja с exit 0 (claude-code#38211); Codex не получает exit-code cmake на
   Windows (#14453); Bazel fetch-403 (gateway, #77610)] [ninja/make docs, job-object/
   PDEATHSIG] [ninja -t recompact, проверка mtime/файлов-выводов] [synthetic + historical].

### S. Debugging — НОВЫЙ ДОМЕН (домен debugging)

52. **debugging-crash-triage-discipline** — [«мерри-го-раунд» гипотез: фикс ломает другое
   (Gemini-CLI-парсер, 2025); wrong-layer диагноз + ложное «fixed» (claude-code#78133);
   переписан весь lighting из-за битой текстуры (mropert, 2026); «доверять месту ошибки»
   ≠ корень] [GDB manual, crash-triage методики] [gdb + core dump + воспроизведение/
   минимизация] [synthetic + FP].

53. **debugging-instrumentation-over-reasoning** — [дебаггер/логика не нашли баг, помог
   printf-трейсинг в файл (Pypersistent, 2026); tail-усечение стектрейсов → цикл
   повторных прогонов (codemine, 2026); уверенное, но неверное «Dispose()» (localghost)]
   [GDB manual, трассировочные методики] [файловое логирование + gdb + strace] [synthetic
   + FP].

### T. Kernel drivers / модули (домен kernel — дополнение)

54. **kernel-driver-char-device-lifecycle** — [file_operations-контракты, неограниченный
   `copy_from_user` в стек (ChatGPT-демо, 2022), инвертированный read/write-контракт,
   двойная class_destroy на unload] [LDD3, Documentation/driver-api] [компиляция + QEMU +
   load/unload тест] [synthetic + FP].

55. **kernel-module-build-out-of-tree** — [Kbuild/Kconfig, kernel-headers зависимость
   (Codex macbook12 PR#5, 2025), MODULE_LICENSE, версионная привязка к ядру]
   [Documentation/kbuild] [make + QEMU + modprobe] [synthetic + historical].

56. **kernel-api-drift-migration** — [удаление `sys_call_table` из экспорта (6.9+): хук
   молча не срабатывает без ошибок (StackOverflow, 2024); DRM fbdev→client_setup, IIO-
   миграции (Claude PR, 2026)] [kernel source, docs.kernel.org] [компиляция против
   пинованного ядра] [synthetic + historical].

### U. Side-channel & formal verification (домены security, compiler)

57. **side-channel-constant-time-verification** — [тайминг-утечки: деление, табличные
   доступы по секрету, early-exit сравнения, string `==` (CWE-1254, ~40% уязвимых у
   Copilot); UDIV/SDIV-тайминг ML-DSA → CVE-2026-22705; «no constant-time claim»-листы
   (rscrypto)] [dudect, ctgrind (BearSSL), ToB constant-time-analysis — как идея]
   [dudect/ctgrind или тайминг-анализ ассемблера] [synthetic + historical (CVE-2026-22705)
   + FP].

58. **formal-spec-loop-invariants** — [LLM-спеки «обманывают» проверы: vacuous/неверные
   инварианты, -20% точности после отсева (LiveFMBench, 2605.01394); ремонт инвариантов
   только 16% (2511.06552); Kani-спеки наследуют баги реализации (KaPilot)]
   [ACSL, CBMC, Frama-C, Kani] [Frama-C/CBMC/Kani] [synthetic + FP].

59. **smt-z3-sound-usage** — [несостоятельные аксиомы LLM→Z3 (ProofOfThought, FP-риск);
   «prover прошёл» ≠ верно; confidence ≠ correctness при символьной проверке протоколов
   (ProVerif/OFMC, 2607.20712)] [Z3 docs, SMT-LIB] [z3 python + контраргумент-проверка]
   [synthetic + adversarial].

### V. Embedded: bringup / flash / OTA / HIL (домен embedded — дополнение)

60. **embedded-board-bringup-peripheral-init** — [quadrature-энкодер «выглядит правильно,
   но неверен» (mcuoneclipse, 2025); init-порядок/clock-tree/GPIO; «угадывание» регистров
   obscure-MCU даже в одной сессии] [datasheet'ы примеров, ARM CMSIS] [компиляция +
   эмуляция + проверка конфигов] [synthetic + FP].

61. **embedded-flash-debug-cycle** — [OpenOCD/GDB-сироты держат SWD-пробу (stm32-gdb-mcp
   #30/#48, 3 инцидента/день); probe-contention и лок-файлы; DFU-stranded + WinUSB-preflight
   (openmotion #95); «не могу надёжно прошить» (stm32-mcp)] [OpenOCD docs, GDB remote]
   [openocd + arm-none-eabi-gdb + real/эмулированный probe] [synthetic + historical].

62. **embedded-ota-bootloader-safety** — [брик флота bad OTA-конфигами (ESP32-S3, 2026);
   staged rollout, trial-window, rollback-слот; FreeRTOS: `break` в чужом цикле → task
   return = fatal] [MCUboot docs, esp-idf OTA docs] [esp-idf/MCUboot + QEMU] [synthetic +
   FP].

63. **embedded-hil-ci-testing** — [HIL-CI: device-ready race, re-enumeration после флеша,
   flash-verify (openmotion #164); ESP-IDF `CLAUDE.md`/Zephyr `copilot-instructions.md` —
   экосистемные гайды против AI-мисюза тулчейна] [Zephyr twister, esp-idf CI docs]
   [west/idf.py + QEMU или железо] [synthetic + FP].

### W. Rust unsafe (домен rust — дополнение)

64. **rust-unsafe-safety-contract-verification** — [сфабрикованные SAFETY-комментарии
   оправдывают UAF (Bun PathString.rs, 2026): инвариант «caller guarantees…» не существует
   (нет PhantomData/lifetime); «SAFETY-блоки — ложь»] [Rust Reference, Rustonomicon]
   [cargo check + Miri/ASan] [synthetic + FP].

> Порядок создания: сначала A (ассемблер — 5 скиллов), затем J (zig — 11), затем
> R (build-systems — 4) и S (debugging — 2), затем остальные по доменам. Приоритет
> source-backed-возможности не ниже «документарно».

## 6. ЕДИНИЦА РАБОТЫ (как создавать скилл)

Для каждого скилла из §5 выполни последовательность:
1. **DISCOVERED**: зарегистрируй в `registry/skills.yaml` (status: discovered,
   stability: draft) и `registry/cross-links.yaml` (связи с существующими скиллами).
2. **SOURCE-BACKED research**: собери первичные источники из ресёрчей (§5 уже даёт
   id/URL); добавь в `registry/sources.yaml`; сформулируй claim'ы в
   `registry/claims.yaml` (claim → source → section).
3. **IMPLEMENTED**: создай `SKILL.md` + `references/` + `examples/good|bad` +
   `evals/README.md`. Примеры — реально скомпилированные/исполненные; для bad —
   задокументируй, почему неверно.
4. **VERIFIED**: прогнай примеры на тулчейне из §5; запиши фактический результат
   (disasm-вывод, вывод фаззера, exit code) в verified facts evals/README.md.
5. Обнови `status` в skills.yaml: verified, если тулчейн доступен; `researched`,
   если только документарно.
6. **TOKEN-OPTIMIZED**: проверь размер SKILL.md (≤ ~400 строк), references по
   требованию.

## 7. ИСПОЛНЕНИЕ И БАТЧИ (если субагенты)

Разбей 64 скилла на батчи по 5–7 (субагенты с self-contained промптами):
- Батч 1: A (asm, 5 скиллов) — приоритет HIGH.
- Батч 2: J (zig, 11 скиллов) — приоритет HIGH (новый домен).
- Батч 3: B + C + D (binary/RE 5 + concurrency 1 + rust 3).
- Батч 4: E + F + G + H (embedded 2 + kernel 3 + networking 1 + sanitizers 1).
- Батч 5: I + K + L + M (gpu 2 + virt 1 + riscv 2 + hpc 3).
- Батч 6: N + O + P + Q (hdl 3 + mobile 2 + uefi 1 + meta 1).
- Батч 7: R (build-systems, 4 скилла) — HIGH (новый домен).
- Батч 8: S + T (debugging 2 + kernel-driver 3).
- Батч 9: U + V + W (side-channel/formal 3 + embedded-cycle 4 + rust-contract 1).

Каждый субагент: читает AGENTS.md, регистрирует скиллы, создаёт файлы, прогоняет
валидаторы для своих файлов, возвращает отчёт о верификации.

## 8. КРИТЕРИИ ПРИЁМКИ (все обязательны)

- В `registry/skills.yaml`: все 64 новых скилла зарегистрированы, статусы
  соответствуют фактической верификации.
- `tools/lint/skill_lint.py` → OK для всех 124 SKILL.md (60 + 64).
- `tools/lint/registry_check.py` → 0 ошибок.
- `tools/source/source_check.py` → 0 WARN (каждый claim трассируется).
- В каждом evals/README.md есть кейсы всех 4 категорий ИЛИ честная пометка, какие
  недоступны и почему.
- Ни один новый скилл не пересекается с существующими 60 (проверь cross-links.yaml).
- Количественные данные из ресёрчей присутствуют как verified facts (с источником).
- `roadmap/progress.yaml` и `WORKLOG.md` обновлены.

## 9. ЗАПРЕТЫ

- Не выдумывай источники/цифры/URL: всё берётся из `research/*.md` или проверяемых
  первичных источников (запрещено копировать текст из ресёрчей — это аналитика, а
  не справочник; переформулируй).
- Не создавай скилл без записи в registry (правило №1 AGENTS.md).
- Не заявляй source-backed без реального прогона тулчейна.
- Не дублируй существующие скиллы (§4) — при пересечении >30% предложи дополнение.
- Не копируй текст из внешних репозиториев: только идеи/структура (лицензионная
  сводка — в research/2026-08-15-external-repos-audit.md §5: MIT/Apache/BSD/CC-BY —
  переписывание с атрибуцией; CC-BY-SA и безлицензионные — только идеи).
