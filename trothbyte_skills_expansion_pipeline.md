# TrothByte Skills Expansion Pipeline
## Пайплайн добавления ~185 новых низкоуровневых скиллов

**Версия:** 1.0  
**Дата:** 2026-08-20  
**Целевой репозиторий:** `https://github.com/TrothByte/low-level-skills-trothbyte`  
**Текущее состояние:** 185 скиллов, 35 доменов, 93 source-backed  
**Цель:** +185 скиллов, полное покрытие 25 доменов

---

## 📋 Правила для агента-разработчика (MUST READ)

> **Перед началом работы агент ОБЯЗАН:**

1. **Прочитать `AGENTS.md`** — engineering rules, resume protocol, scope management
2. **Прочитать `CONTRIBUTING.md`** — contribution guidelines, PR process, quality gates
3. **Прочитать `docs/ARCHITECTURE.md`** (или аналог) — структура репозитория
4. **Изучить 3-5 существующих `SKILL.md`** из разных доменов (например: `skills/c/c-undefined-behavior/SKILL.md`, `skills/kernel/kernel-uaccess-safety/SKILL.md`, `skills/security/side-channel-constant-time-verification/SKILL.md`)
5. **Запустить `tools/validate.py`** и убедиться, что CI проходит на чистом репозитории
6. **Измерить токены** каждого нового `SKILL.md` через `tools/tokens/token_measure.py` — хард-лимит ≤2000 tokens

### Стандарты формата SKILL.md

Каждый скилл должен следовать структуре:

```
skills/<domain>/<skill-name>/
├── SKILL.md                    # ≤2000 tokens, operational, loads first
├── references/
│   ├── README.md               # deep knowledge, loads on demand
│   ├── sources.yaml            # claim → source → section mapping
│   └── <topic>.md              # дополнительные deep-dive
├── examples/
│   ├── good/
│   │   └── <example>.c         # compiled and run
│   └── bad/
│       └── <example>.c         # compiled and run, demonstrates failure
└── evals/
    ├── README.md               # eval strategy
    ├── synthetic/
    ├── false-positive/
    ├── adversarial/
    └── historical-cve/
```

### Каждый SKILL.md обязан содержать:

1. **When to use** — триггеры загрузки скилла
2. **When NOT to use** — анти-триггеры, предотвращающие overloading
3. **What the agent often gets wrong** — каталогизированные систематические ошибки
4. **How to reason correctly** — позитивный процесс рассуждения
5. **What to verify and how** — executable gates (compile + run + sanitizer + asm inspect)
6. **Where the knowledge comes from** — traceable primary sources

### Claim extraction (registry/)

Каждый нормативный claim должен быть вынесен в `registry/claims.yaml`:
```yaml
- id: <skill-name>-<n>
  claim: "<exact claim text>"
  source_id: <source-ref>
  section: "<section>"
  skill: <skill-name>
  confidence: source-backed | researched | inferred
```

### Verification levels

| Уровень | Требование |
|---------|-----------|
| `source-backed` | Компиляция + запуск на реальном тулчейне (GCC 16.1, rustc 1.97.1, GDB 17.2) |
| `researched` | Основано на primary sources; тулчейн недоступен в dev-окружении — честно помечено |
| `evaluated` | Пройдены synthetic + false-positive + adversarial evals |

---

## 🗺️ Фазированный план (6 фаз, ~31 недель)

### Фаза 1: Инфраструктура и критические ядро-скиллы (Недели 1-4)
**Цель:** Заложить фундамент, покрыть наиболее частые источники kernel panics и data loss.

| # | Скилл | Домен | Приоритет | Стабильность |
|---|-------|-------|-----------|-------------|
| 1.1 | `vfs-file-operations-and-fops` | kernel | CRITICAL | source-backed |
| 1.2 | `page-cache-writeback-semantics` | kernel | CRITICAL | source-backed |
| 1.3 | `workqueue-flush-and-cancellation` | kernel | CRITICAL | source-backed |
| 1.4 | `kernel-timers-hrtimer-vs-legacy` | kernel | CRITICAL | source-backed |
| 1.5 | `waitqueue-completion-synchronization` | kernel | HIGH | source-backed |
| 1.6 | `kthread-create-and-teardown` | kernel | HIGH | source-backed |
| 1.7 | `sk-buff-socket-buffer-management` | networking | CRITICAL | source-backed |
| 1.8 | `tcp-congestion-control-internals` | networking | CRITICAL | source-backed |

**Чеклист фазы 1:**
- [ ] Все SKILL.md ≤2000 tokens (проверить `token_measure.py`)
- [ ] Все примеры компилируются и запускаются
- [ ] Claims вынесены в `registry/claims.yaml`
- [ ] `tools/validate.py` проходит без ошибок
- [ ] PR проходит CI (`.github/workflows/ci.yml`)
- [ ] Документация обновлена (`docs/SKILLS.md` регенерирован)

---

### Фаза 2: Конкурентность, микроархитектура и безопасность (Недели 5-8)
**Цель:** Покрыть lock-free, CPU security, speculative execution — домены с наибольшим security impact.

| # | Скилл | Домен | Приоритет | Стабильность |
|---|-------|-------|-----------|-------------|
| 2.1 | `userspace-rcu-read-side-critical-sections` | concurrency | CRITICAL | source-backed |
| 2.2 | `hazard-pointers-memory-reclamation` | concurrency | HIGH | source-backed |
| 2.3 | `epoch-based-reclamation-ebr` | concurrency | HIGH | source-backed |
| 2.4 | `seqlock-reader-writer-optimistic` | concurrency | HIGH | source-backed |
| 2.5 | `futex-fast-userspace-mutex` | concurrency | HIGH | source-backed |
| 2.6 | `speculative-execution-mitigations` | security | CRITICAL | source-backed |
| 2.7 | `cache-coherency-protocols-mesi-moesi` | performance | HIGH | source-backed |
| 2.8 | `intel-cet-shadow-stack` | security | HIGH | source-backed |
| 2.9 | `amd-sev-snp-confidential-computing` | security | HIGH | researched |
| 2.10 | `arm-pac-bti-pointer-authentication` | security | HIGH | researched |
| 2.11 | `constant-time-assembly-verification` | security | CRITICAL | source-backed |
| 2.12 | `mcs-lock-scalable-spinlock` | concurrency | MEDIUM | source-backed |

---

### Фаза 3: Rust/C++ глубже, языковые рантаймы, формальные методы (Недели 9-13)
**Цель:** Покрыть async Rust, no_std, C++ coroutines, Go/Java/Python FFI, и начать formal methods.

| # | Скилл | Домен | Приоритет | Стабильность |
|---|-------|-------|-----------|-------------|
| 3.1 | `rust-async-executor-and-waker-design` | rust | CRITICAL | source-backed |
| 3.2 | `rust-pin-and-self-referential-structs` | rust | HIGH | source-backed |
| 3.3 | `rust-no-std-and-embedded-alloc` | rust | HIGH | source-backed |
| 3.4 | `rust-miri-undefined-behavior-detection` | rust | HIGH | source-backed |
| 3.5 | `rust-polonius-borrow-checker` | rust | MEDIUM | researched |
| 3.6 | `rust-custom-allocators-api` | rust | MEDIUM | source-backed |
| 3.7 | `cpp20-coroutines-co-await-co-yield` | cpp | HIGH | source-backed |
| 3.8 | `cpp20-modules-and-build-system-integration` | cpp | MEDIUM | researched |
| 3.9 | `cpp-memory-model-acquire-release-deep` | cpp | HIGH | source-backed |
| 3.10 | `cpp-allocator-traits-and-pmr` | cpp | MEDIUM | source-backed |
| 3.11 | `cpp-exception-handling-itanium-abi` | cpp | MEDIUM | source-backed |
| 3.12 | `go-runtime-gc-and-scheduler-internals` | runtime | HIGH | researched |
| 3.13 | `go-cgo-ffi-boundary-safety` | runtime | CRITICAL | source-backed |
| 3.14 | `java-jni-global-local-reference-management` | runtime | HIGH | source-backed |
| 3.15 | `java-jvm-safepoint-and-thread-state` | runtime | MEDIUM | researched |
| 3.16 | `python-c-api-reference-counting` | runtime | HIGH | source-backed |
| 3.17 | `ocaml-runtime-gc-and-ffi` | runtime | MEDIUM | researched |
| 3.18 | `lua-c-api-stack-management` | runtime | MEDIUM | source-backed |
| 3.19 | `coq-proof-assistant-basics` | formal | HIGH | researched |
| 3.20 | `isabelle-hol-proof-methodology` | formal | HIGH | researched |
| 3.21 | `tla-plus-temporal-logic-specs` | formal | HIGH | researched |
| 3.22 | `separation-logic-heap-reasoning` | formal | HIGH | researched |
| 3.23 | `iris-concurrent-separation-logic` | formal | HIGH | researched |
| 3.24 | `dafny-verification-aware-programming` | formal | MEDIUM | researched |
| 3.25 | `cbmc-bounded-model-checking-c` | formal | MEDIUM | researched |
| 3.26 | `frama-c-acsl-contracts-deep` | formal | MEDIUM | researched |

---

### Фаза 4: Сети, хранение, шины, периферия (Недели 14-19)
**Цель:** Покрыть kernel networking, filesystems, block layer, NVMe, шины (I2C/SPI/UART/CAN).

| # | Скилл | Домен | Приоритет | Стабильность |
|---|-------|-------|-----------|-------------|
| 4.1 | `netfilter-nftables-packet-path` | networking | HIGH | source-backed |
| 4.2 | `xdp-zero-copy-networking` | networking | HIGH | researched |
| 4.3 | `dpdk-packet-processing` | networking | HIGH | researched |
| 4.4 | `netlink-socket-and-rtnl` | networking | HIGH | source-backed |
| 4.5 | `socket-options-tcp-tune` | networking | MEDIUM | source-backed |
| 4.6 | `ipv6-extension-headers-and-fragmentation` | networking | MEDIUM | source-backed |
| 4.7 | `wireguard-protocol-implementation` | networking | MEDIUM | researched |
| 4.8 | `napi-network-driver` | networking | HIGH | researched |
| 4.9 | `ext4-journaling-and-recovery` | kernel | HIGH | source-backed |
| 4.10 | `fuse-filesystem-driver` | kernel | HIGH | source-backed |
| 4.11 | `block-layer-and-io-scheduler` | kernel | HIGH | source-backed |
| 4.12 | `nvme-protocol-and-admin-commands` | kernel | HIGH | researched |
| 4.13 | `dm-crypt-luks-crypto-mapping` | kernel | MEDIUM | researched |
| 4.14 | `flash-translation-layer-nand` | embedded | MEDIUM | researched |
| 4.15 | `i2c-driver-writing-and-protocol` | embedded | HIGH | source-backed |
| 4.16 | `spi-driver-cs-and-mode-setup` | embedded | HIGH | source-backed |
| 4.17 | `uart-serial-driver-and-line-discipline` | embedded | MEDIUM | source-backed |
| 4.18 | `can-bus-protocol-and-socketcan` | embedded | MEDIUM | source-backed |
| 4.19 | `sdio-wifi-bt-driver-interface` | embedded | MEDIUM | researched |
| 4.20 | `sata-ahci-driver-basics` | embedded | LOW | researched |
| 4.21 | `modbus-rtu-tcp-protocol` | embedded | LOW | researched |
| 4.22 | `1-wire-ds18b20-protocol` | embedded | LOW | researched |
| 4.23 | `usb-device-stack` | embedded | MEDIUM | researched |
| 4.24 | `pcie-config-space` | embedded | MEDIUM | researched |

---

### Фаза 5: GPU, компиляторы, firmware, мультимедиа (Недели 20-25)
**Цель:** Покрыть GPU memory management, Mesa, compiler backends, firmware, audio/video.

| # | Скилл | Домен | Приоритет | Стабильность |
|---|-------|-------|-----------|-------------|
| 5.1 | `gpu-memory-management-gem-ttm` | gpu | HIGH | researched |
| 5.2 | `drm-kms-display-pipeline` | gpu | HIGH | researched |
| 5.3 | `mesa-shader-compiler-nir` | gpu | MEDIUM | researched |
| 5.4 | `opengl-driver-state-tracking` | gpu | MEDIUM | researched |
| 5.5 | `cuda-driver-api-vs-runtime` | gpu | MEDIUM | researched |
| 5.6 | `rocm-hip-kernel-development` | gpu | MEDIUM | researched |
| 5.7 | `dma-buf-cross-device-sharing` | gpu | HIGH | researched |
| 5.8 | `vulkan-compute-shaders` | gpu | MEDIUM | researched |
| 5.9 | `ptx-assembly` | gpu | MEDIUM | researched |
| 5.10 | `register-allocation-graph-coloring` | compiler | MEDIUM | researched |
| 5.11 | `instruction-selection-and-lowering` | compiler | MEDIUM | researched |
| 5.12 | `link-time-optimization-lto` | compiler | MEDIUM | source-backed |
| 5.13 | `profile-guided-optimization-pgo` | compiler | MEDIUM | source-backed |
| 5.14 | `jit-compilation-basics` | compiler | HIGH | source-backed |
| 5.15 | `dwarf-expression-evaluation` | compiler | MEDIUM | source-backed |
| 5.16 | `stack-unwinding-and-exceptions` | compiler | MEDIUM | source-backed |
| 5.17 | `coreboot-romstage-ramstage` | firmware | HIGH | researched |
| 5.18 | `u-boot-driver-model-and-fit-image` | firmware | HIGH | researched |
| 5.19 | `acpi-dsdt-ssdt-table-parsing` | firmware | MEDIUM | researched |
| 5.20 | `intel-me-amd-psp-firmware-security` | firmware | MEDIUM | researched |
| 5.21 | `bmc-ipmi-redfish-server-management` | firmware | MEDIUM | researched |
| 5.22 | `system-management-mode-smm` | firmware | LOW | researched |
| 5.23 | `alsa-pcm-capture-playback` | multimedia | MEDIUM | source-backed |
| 5.24 | `v4l2-video-capture-and-m2m` | multimedia | MEDIUM | source-backed |
| 5.25 | `video-codec-h264-hevc-av1-lowlevel` | multimedia | MEDIUM | researched |
| 5.26 | `wayland-protocol-compositor` | multimedia | MEDIUM | researched |
| 5.27 | `gstreamer-pipeline-and-pads` | multimedia | LOW | researched |

---

### Фаза 6: Оставшиеся домены и финальная полировка (Недели 26-31)
**Цель:** Базы данных, distributed systems, virtualization, mobile, стандарты, нишевые языки, отладка, power management, time/clock.

| # | Скилл | Домен | Приоритет | Стабильность |
|---|-------|-------|-----------|-------------|
| 6.1 | `b-tree-page-layout-and-splitting` | database | HIGH | source-backed |
| 6.2 | `lsm-tree-compaction-and-leveling` | database | HIGH | researched |
| 6.3 | `write-ahead-logging-wal-recovery` | database | CRITICAL | source-backed |
| 6.4 | `mvcc-snapshot-isolation-implementation` | database | HIGH | researched |
| 6.5 | `lock-free-skip-list-implementation` | database | MEDIUM | source-backed |
| 6.6 | `page-pool-and-buffer-management` | database | MEDIUM | source-backed |
| 6.7 | `raft-consensus-implementation` | distributed | CRITICAL | source-backed |
| 6.8 | `paxos-multi-paxos-optimization` | distributed | HIGH | researched |
| 6.9 | `distributed-tracing-opentelemetry` | distributed | MEDIUM | researched |
| 6.10 | `byzantine-fault-tolerance-pbft` | distributed | MEDIUM | researched |
| 6.11 | `consistent-hashing-and-dht` | distributed | MEDIUM | source-backed |
| 6.12 | `virtio-vhost-vhost-user-protocol` | virtualization | HIGH | researched |
| 6.13 | `container-runtime-runc-crun-oci` | virtualization | HIGH | researched |
| 6.14 | `seccomp-bpf-policy-authoring` | virtualization | HIGH | source-backed |
| 6.15 | `kata-containers-vm-isolation` | virtualization | MEDIUM | researched |
| 6.16 | `aws-firecracker-microvm` | virtualization | MEDIUM | researched |
| 6.17 | `gvisor-sentry-gofer-protocol` | virtualization | LOW | researched |
| 6.18 | `android-binder-driver-and-aidl` | mobile | HIGH | researched |
| 6.19 | `android-hal-hidl-aidl-migration` | mobile | MEDIUM | researched |
| 6.20 | `ios-xnu-kernel-programming` | mobile | HIGH | researched |
| 6.21 | `mach-o-binary-format-and-dyld` | mobile | MEDIUM | source-backed |
| 6.22 | `ios-jailbreak-exploit-mitigations` | mobile | LOW | researched |
| 6.23 | `do-178c-avionics-software` | standards | HIGH | researched |
| 6.24 | `iso-26262-automotive-safety` | standards | HIGH | researched |
| 6.25 | `common-criteria-eal-evaluation` | standards | MEDIUM | researched |
| 6.26 | `fips-140-3-cryptographic-module` | standards | MEDIUM | researched |
| 6.27 | `ada-spark-safety-critical-programming` | niche-lang | HIGH | researched |
| 6.28 | `modern-fortran-interoperability` | niche-lang | LOW | researched |
| 6.29 | `dlang-betterc-and-memory-safety` | niche-lang | LOW | researched |
| 6.30 | `nim-memory-management-arc-orc` | niche-lang | LOW | researched |
| 6.31 | `carbon-language-cpp-interop` | niche-lang | LOW | researched |
| 6.32 | `perf-profiling-and-flame-graphs` | debugging | HIGH | source-backed |
| 6.33 | `bpftrace-one-liners-and-scripts` | debugging | MEDIUM | source-backed |
| 6.34 | `valgrind-memcheck-cachegrind-callgrind` | debugging | MEDIUM | source-backed |
| 6.35 | `rr-record-replay-debugging` | debugging | MEDIUM | researched |
| 6.36 | `lldb-python-scripting-api` | debugging | LOW | researched |
| 6.37 | `windbg-kernel-debugging-windows` | debugging | LOW | researched |
| 6.38 | `strace-ltrace-syscall-tracing` | debugging | MEDIUM | source-backed |
| 6.39 | `cpufreq-cpuidle-governor-selection` | power | HIGH | source-backed |
| 6.40 | `acpi-power-states-suspend-resume` | power | HIGH | researched |
| 6.41 | `arm-psci-power-state-coordination` | power | HIGH | researched |
| 6.42 | `thermal-management-and-throttling` | power | MEDIUM | researched |
| 6.43 | `tickless-kernel-and-hrtimer-dyntick` | power | MEDIUM | source-backed |
| 6.44 | `timekeeping-monotonic-vs-realtime` | time | HIGH | source-backed |
| 6.45 | `tsc-hpet-acpi-pm-timer-selection` | time | MEDIUM | source-backed |
| 6.46 | `ntp-ptp-kernel-timestamping` | time | MEDIUM | researched |
| 6.47 | `userspace-timer-wheel-hierarchical` | time | MEDIUM | source-backed |
| 6.48 | `fixed-point-arithmetic-embedded` | math | MEDIUM | source-backed |
| 6.49 | `arbitrary-precision-arithmetic` | math | MEDIUM | source-backed |
| 6.50 | `fft-implementation-and-optimization` | math | LOW | source-backed |
| 6.51 | `blas-matrix-multiplication-optimization` | math | LOW | source-backed |
| 6.52 | `io-uring-advanced-patterns` | concurrency | HIGH | researched |
| 6.53 | `lock-free-hash-tables-and-skiplists` | concurrency | MEDIUM | source-backed |
| 6.54 | `tpm2-tss-programming-and-attestation` | security | CRITICAL | researched |
| 6.55 | `dice-measured-boot-and-layered-attestation` | security | CRITICAL | researched |
| 6.56 | `tls-1-3-handshake-and-record-layer` | security | HIGH | researched |
| 6.57 | `x509-certificate-chain-validation` | security | HIGH | source-backed |
| 6.58 | `aes-ni-sha-ni-intrinsics` | security | HIGH | source-backed |
| 6.59 | `exploit-mitigation-aslr-cfi-pac` | security | HIGH | source-backed |
| 6.60 | `fuzzing-afl-libfuzzer-structure-aware` | security | HIGH | source-backed |
| 6.61 | `secure-enclave-sgx-tdx` | security | HIGH | researched |
| 6.62 | `memory-tagging-extension-mte-auditing` | security | MEDIUM | researched |
| 6.63 | `kernel-module-parameters-and-symbols` | kernel | MEDIUM | source-backed |
| 6.64 | `kernel-memory-hotplug-and-ballooning` | kernel | MEDIUM | researched |
| 6.65 | `kernel-ftrace-perf-tracepoints` | kernel | MEDIUM | source-backed |
| 6.66 | `kernel-kprobes-uprobes` | kernel | MEDIUM | source-backed |
| 6.67 | `kernel-cgroup-v2-resource-control` | kernel | MEDIUM | researched |
| 6.68 | `kernel-livepatch-klp` | kernel | LOW | researched |
| 6.69 | `sysfs-debugfs-kobject-lifecycle` | kernel | HIGH | source-backed |
| 6.70 | `page-table-management` | kernel | HIGH | source-backed |
| 6.71 | `property-based-testing-kernel` | kernel | HIGH | source-backed |
| 6.72 | `sel4-sddf-driver-framework` | kernel | HIGH | researched |
| 6.73 | `toctou-kernel` | kernel | HIGH | researched |
| 6.74 | `kernel-exploitation-primitives` | kernel | HIGH | researched |
| 6.75 | `kernel-loader-elf` | kernel | HIGH | source-backed |
| 6.76 | `kernel-patch-review-commit-log-independence` | kernel | HIGH | researched |
| 6.77 | `kernel-scheduler-mm-vfs-internals` | kernel | HIGH | researched |
| 6.78 | `kernel-ub-patterns` | kernel | HIGH | source-backed |
| 6.79 | `kernel-driver-char-device-lifecycle` | kernel | HIGH | researched |
| 6.80 | `kernel-debugging-ftrace-kprobes-kdump` | kernel | HIGH | researched |
| 6.81 | `kernel-module-build-out-of-tree` | kernel | HIGH | researched |
| 6.82 | `kernel-container-internals` | kernel | HIGH | researched |
| 6.83 | `kernel-api-drift-migration` | kernel | HIGH | researched |
| 6.84 | `iommu-smmu-isolation` | kernel | HIGH | researched |
| 6.85 | `interrupt-controller-gic-apic` | kernel | HIGH | researched |
| 6.86 | `dma-cache-coherency` | kernel | HIGH | source-backed |
| 6.87 | `data-race-kernel-detection` | kernel | HIGH | source-backed |
| 6.88 | `deadlock-kernel-prevention` | kernel | HIGH | source-backed |
| 6.89 | `framekernel-architecture` | kernel | HIGH | researched |
| 6.90 | `fuzzing-harness-kernel` | kernel | HIGH | source-backed |

---

## 🔧 Рабочий процесс для агента (Workflow)

### Шаг 1: Подготовка (Prep)
```bash
# 1.1 Клонировать репозиторий
git clone https://github.com/TrothByte/low-level-skills-trothbyte
cd low-level-skills-trothbyte

# 1.2 Убедиться, что dev-окружение работает
python3 tools/validate.py --check-all

# 1.3 Изучить стандарты
cat AGENTS.md
cat CONTRIBUTING.md
cat docs/ARCHITECTURE.md  # или аналог

# 1.4 Изучить 3-5 эталонных скиллов
ls skills/c/c-undefined-behavior/
ls skills/kernel/kernel-uaccess-safety/
ls skills/security/side-channel-constant-time-verification/
```

### Шаг 2: Разработка скилла (Develop)
```bash
# 2.1 Создать структуру
mkdir -p skills/<domain>/<skill-name>/{references,examples/{good,bad},evals/{synthetic,false-positive,adversarial,historical-cve}}

# 2.2 Написать SKILL.md (≤2000 tokens)
# Следовать шаблону из AGENTS.md / существующих скиллов

# 2.3 Написать references/
# Каждый нормативный claim → primary source

# 2.4 Написать examples/
# good/ — компилируется, демонстрирует правильный подход
# bad/ — компилируется (или нет), демонстрирует типичную ошибку

# 2.5 Написать evals/
# synthetic/ — сгенерированные тесты
# false-positive/ — случаи, когда скилл НЕ должен срабатывать
# adversarial/ — попытки сломать рассуждение
# historical-cve/ — реальные CVE, которые скилл предотвращает
```

### Шаг 3: Верификация (Verify)
```bash
# 3.1 Проверить токены
python3 tools/tokens/token_measure.py skills/<domain>/<skill-name>/SKILL.md
# Должно быть ≤2000

# 3.2 Скомпилировать примеры
cd skills/<domain>/<skill-name>/examples/good
gcc -Wall -Wextra -Werror -fsanitize=address,undefined -O2 example.c -o example_good
./example_good

cd ../bad
gcc -Wall -Wextra -Werror -fsanitize=address,undefined -O2 example.c -o example_bad
# Ожидается: падение санитайзера или неверный вывод

# 3.3 Запустить валидацию
python3 tools/validate.py --skill <skill-name>

# 3.4 Извлечь claims
python3 tools/claims/claim_extractor.py skills/<domain>/<skill-name>/
# Проверить, что claims добавлены в registry/claims.yaml
```

### Шаг 4: Регистрация (Register)
```bash
# 4.1 Добавить скилл в registry/skills.yaml
# 4.2 Обновить docs/SKILLS.md через генератор
python3 tools/reports/gen_skills_catalog.py
# 4.3 Обновить roadmap/
# 4.4 Записать в WORKLOG.md
```

### Шаг 5: PR и Review
```bash
# 5.1 Создать ветку
git checkout -b feat/add-<skill-name>

# 5.2 Коммит с conventional commits
git commit -m "feat(skills): add <skill-name> skill

- Covers <what it covers>
- <n> source-backed claims
- Verified with <toolchain>"

# 5.3 Убедиться, что CI проходит
# 5.4 Запросить review
```

---

## 📊 Метрики успеха

| Метрика | Целевое значение |
|---------|-----------------|
| Всего скиллов | ≥370 (185 + 185) |
| Source-backed | ≥180 (50%+) |
| Researched | ≤170 (честно помечено) |
| Средний размер SKILL.md | ≤2000 tokens |
| Примеры, компилирующиеся | 100% |
| Claims в registry | 100% нормативных |
| CI pass rate | 100% |
| Eval coverage | synthetic + FP + adversarial для каждого CRITICAL/HIGH |

---

## ⚠️ Частые ошибки агентов (Anti-patterns)

1. **"It compiles" ≠ correct** — требовать запуск + sanitizer
2. **Галлюцинация primary sources** — каждый claim должен иметь URL/ISBN/DOI
3. **Превышение токен-бюджета** — глубокое знание идёт в `references/`, не в `SKILL.md`
4. **Отсутствие `bad/` примеров** — без демонстрации ошибки скилл неполноценен
5. **Неправильная классификация stability** — не маркировать `source-backed` если не было компиляции
6. **Дублирование** — проверять `registry/skills.yaml` перед созданием
7. **Забыть обновить `docs/SKILLS.md`** — каталог должен быть актуален
8. **Отсутствие evals** — каждый CRITICAL/HIGH скилл должен иметь adversarial eval

---

## 📝 Примечания

- **Порядок внутри фазы** может меняться в зависимости от зависимостей между скиллами
- **Некоторые скиллы** могут быть объединены, если пересекаются >70%
- **Новые домены** создаются по мере необходимости в `skills/<new-domain>/`
- **Пересмотр** пайплайна проводится после каждой фазы
- **Критерий завершения фазы** — все чеклисты отмечены и CI зелёный

---

*Сгенерировано на основе gap-анализа репозитория TrothByte/low-level-skills-trothbyte*
