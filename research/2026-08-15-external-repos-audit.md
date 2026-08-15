# Audit: внешние low-level skill-репозитории vs TrothByte

**Дата:** 2026-08-15 · **Статус:** исходная аналитика для новых доменов/скиллов и
сверки методологии · **Репозиторий:** TrothByte/low-level-skills-trothbyte

---

## 1. Дата и метод исследования

**Дата проведения:** 2026-08-15.

**Метод.** Обновление списка кандидатов и распределённый аудит 5 параллельными
субагентами. Каждый субагент был обязан **фактически открывать файлы** (`gh api`
contents/git-trees, raw.githubusercontent.com, LICENSE, README, 2–3 SKILL.md на репо),
а не опираться на README/звёзды. Якорные 18 репозиториев проверены на
существование/активность через GitHub API (все живы).

| Субагент | Кластер | Репозитории |
|---|---|---|
| A | **Zig** | rudedogg/zig-skills, zigcc/skills, nzrsky/zig-skills, whit3rabbit/claude-zig-skill, zig-подмножество mohitmishra786 |
| B | **Специфичные ниши** | CSS-Electronics/can-bus-reverse-engineering-skills, NVIDIA/skills (DOCA/DPU), SimoneAvogadro/android-reverse-engineering-skill, SnailSploit/Claude-Red |
| C | **Компиляторы/архитектура/perf** | mohitmishra786/low-level-dev-skills (142 скилла, дерево просканировано целиком) |
| D | **Методология security-review** | trailofbits/skills, wshobson/agents, Jeffallan/claude-skills, P4nda0s/reverse-skills, Masriyan/Claude-Code-CyberSecurity-Skill |
| E | **Новые ниши** | `gh search repos` по topics, VoltAgent/awesome-agent-skills, claudemarketplaces.com, skills.sh, + точечная проверка найденного |

**Важные факты фазы 1:**
- `VoltAgent/awesome-agent-skills` (30,283★, свежий) — каталог-индекс (только README +
  LICENSE + CONTRIBUTING), не набор скиллов.
- `trailofbits/skills` (6,587★) использует структуру `plugins/<name>/` с
  `.claude-plugin/plugin.json`; `wshobson/agents` (38,811★) — `.agents/`,
  `.claude-plugin/`, `.cursor-plugin/`; `zhaoxuya520/reverse-skill` (25,222★) — крупный
  CTF/security-набор (~50+ skills).
- `Z3Prover/z3` из исходного списка 18 — это **проект SMT-солвера**, а не skill-репозиторий;
  к аудиту навыков не относится (см. §6).

**Ограничения поиска:**
- Проверялись в первую очередь деревья и raw-файлы через GitHub API; полнотекстовый
  контент репозиториев > 2–3 SKILL.md на репо не перечитывался.
- Лицензия `P4nda0s/reverse-skills`: API возвращает `null` (LICENSE-файла нет в дереве,
  README заявляет MIT) — UNVERIFIED.
- `trailofbits/skills` evals существуют, но запускаются вручную (`claude plugin eval`),
  CI-гейта на них нет.
- `wshobson/agents` Cursor round-trip верифицирован только рецептом, не реальным CLI.
- Часть мелких нишевых репо (E) просмотрена выборочно (README + 1–2 SKILL.md).

---

## 2. Сводная таблица: домен/ниша → источник → аналог → качество → рекомендация

Легенда: ✅ есть аналог · ◐ частично · ✖ нет аналога. Оценка качества источника 1–5
по критериям §Этапа 3 (верификация, источники, progressive disclosure, позитивные
паттерны, актуальность, лицензия, слабые места).

| Ниша/домен | Внешний источник(и) | Аналог в TrothByte | Кач-во | Рекомендация |
|---|---|---|---|---|
| **Zig** | rudedogg (4/5), zigcc (3/5), nzrsky (3/5, MIT), whit3rabbit (3/5, MIT), mohitmishra-zig (2/5, MIT) | ✖ **пробел подтверждён** | 2–4 | **Новый домен `zig`** (п.3.1) |
| **Virtualization / hypervisor** | mohitmishra786 (3 skills, 3.5/5) | ✖ | 3.5 | **Новый домен `virtualization`** (п.3.2) |
| **HPC / MPI / RDMA** | mohitmishra786 (3 skills, 3/5) | ✖ | 3 | **Новый домен `hpc`** (п.3.3) |
| **OS internals: scheduler/MM/VFS** | mohitmishra786 `kernel` (4/5) | ◐ kernel (RCU/uaccess/atomic-context) | 4 | **Доработка kernel** (п.3.4) |
| **Kernel debugging (kgdb/ftrace/kprobes/kdump)** | mohitmishra786 (3.5/5) | ✖ | 3.5 | Доработка kernel (п.3.4) |
| **CPU architecture (pipeline/speculation/TLB)** | mohitmishra786 (5 skills, 3/5) | ◐ assembly (x86-64), abi | 3 | Доработка assembly/abi |
| **eBPF (не только verifier)** | mohitmishra786 (2 skills, 3.5/5) | ◐ networking (ebpf-verifier-reasoning) | 3.5 | Доработка networking |
| **CAN/automotive RE (DBC, cantools, sawtooth)** | CSS-Electronics (4/5, MIT) | ◐ reverse-engineering (auto-re-protocols-beyond-can) | 4 | **Доработка auto-re** (п.3.5) |
| **RDMA/DPU/NIC offload** | NVIDIA/skills (5/5, Apache+CC-BY) | ✖ networking=только eBPF | 5 | **Новый скилл `networking-hardware`** (п.3.6) |
| **Android RE (DEX/smali/ART/JNI/native .so)** | SimoneAvogadro (3/5, Apache-2.0) | ✖ (go-rust-re/type-recovery — native только) | 3 | **Новый домен `mobile-android-re`** (п.3.7) |
| **Shellcode / exploit-dev** | SnailSploit/Claude-Red (2/5, MIT) | ✖ (asm/binary-analysis читают, не эксплуатируют) | 2 | **Пропустить** (только идеи, п.3.8) |
| **CHERI / capability-архитектура** | qwattash/cheri-skills (4/5, BSD-3) | ✖ | 4 | **Новый скилл** (п.3.9) |
| **RISC-V / RVV 1.0 intrinsics** | alexfdez1010/risc-v-skill (4/5, MIT) | ◐ simd (AVX), asm (x86) | 4 | **Новый домен `risc-v`** (п.3.10) |
| **FPGA / HDL (CDC, timing, constraints)** | LNC0831/oh-my-fpga (4/5, MIT) | ✖ | 4 | **Новый домен `hdl`** (OSS-верификация) (п.3.11) |
| **Ghidra-автоматизация (RPC, daemon)** | cellebrite-labs/ghidra-rpc (4/5, **нет лицензии**) | ◐ binary-analysis | 4 | Идея (нет лицензии) |
| **Fuzzing-харнесы (AFL/libFuzzer + sanitizers)** | 0xazanul/fuzz-skill (3.5/5, нет лицензии) | ◐ sanitizers (report-reading/CI-loop) | 3.5 | **Доработка sanitizers** (п.3.12) |
| **Exploit-technique portfolio** | n132/Libc-GOT-Hijacking (3/5, нет лицензии) | ✖ | 3 | Идея (п.3.8) |
| **Malware RE workflows (IOC/unpacking)** | hackersifu (3/5, MIT), sector-b79, rekit (Apache-2.0) | ◐ reverse-engineering | 3 | Низкий приоритет |
| **UEFI/BIOS firmware** | MarsDoge/uefi-firmware-skill (3/5, MIT) | ◐ bootloader (OS-bootloader) | 3 | Доработка bootloader (п.3.13) |
| **Широкий security-набор (817 skills)** | mukul975/Anthropic-Cybersecurity-Skills (27,783★, Apache-2.0) | ◐ RE/security-классы | 3 | Отслеживать: breadth, не depth |
| **r0crawl (196 RE-модулей, router)** | manyuegong33/r0crawl_skills (3.5/5, нет лицензии) | ◐ RE/binary-analysis | 3.5 | Идея (router/beginner-contract) |

### Методология (архитектурный слой, §4)
| Репо | Оценка | Главное |
|---|---|---|
| trailofbits/skills | 5/5 | лучшая верификация (validator `--self-test`, CI c cross-compile aarch64/semgrep/llvm-19), evals с ablation Δ, FP-verdict-таксономия, coverage-gate-файлы |
| wshobson/agents | 4.5/5 | PluginEval (3 уровня, Monte-Carlo активация, статистические CI, Elo), `make garden` drift-detection, real-CLI round-trips |
| Jeffallan/claude-skills | 3.5/5 | валидатор проверяет «контейнер», не «контент»; разовый LLM-аудит нашёл 30+ phantom ссылок |
| Masriyan/…CyberSecurity-Skill | 3/5 | chaining-таблицы «условие → следующий скилл», output-template-first, authorization-gates |
| P4nda0s/reverse-skills | 2/5 | лицензия UNVERIFIED; монолиты; но хорошие negative-guidance-паттерны с обоснованием |

---

## 3. Подробный разбор доменов без аналога — приоритет

### 3.1 Zig — пробел ПОДТВЕРЖДЁН (высший приоритет)

- **Источники:** rudedogg/zig-skills (38★, 0.17.0-dev, **без лицензии**, 4/5),
  zigcc/skills (36★, 0.15/0.16, **без лицензии**, 3/5), nzrsky/zig-skills (10★, MIT, 3/5,
  дериват rudedogg, дублирование 15×), whit3rabbit/claude-zig-skill (23★, MIT, 3/5,
  мультиверсионный, но стейл с дек 2025), mohitmishra-zig (7 скиллов, MIT, 2/5 — контент
  0.13-эры, **не компилируется** на актуальном Zig).
- **Актуальность пробела:** подтверждена. Zig — мост между доменами TrothByte
  (LLVM-бэкенд, comptime, явные аллокаторы, inline asm, C ABI, cross-compile, ELF,
  bare-metal). Ни один из 5 репо не имеет evals/верификации/CI, ни один не пинит
  стабильную актуальную версию с задокументированной методологией.
- **Рекомендация:** новый домен `zig`. Взять как **идею** (не текст):
  - progressive-disclosure-архитектуру и WRONG/CORRECT migration-пары из rudedogg/nzrsky;
  - формальную политику версионирования из zigcc `VERSIONING.md` («update-in-place vs
    new-skill vs multi-version»);
  - мультиверсионное разложение references из whit3rabbit.
  Текст из rudedogg/zigcc копировать нельзя (нет лицензии). Из MIT-репо (nzrsky,
  whit3rabbit, mohitmishra) — только с переписыванием + атрибуцией; примеры mohitmishra
  **нельзя брать вообще** (фактически неверны для текущего Zig).
- **Source-backed топики** (первичка есть): comptime/`@typeInfo`-split, аллокаторы
  `std.heap`, миграции 0.15→0.16→0.17 (std.Io/Writergate, Juicy Main), inline asm/ABI,
  `@cImport`/FFI, build.zig/build.zig.zon, cross-compile `zig cc`, `@Vector`/std.simd,
  error-модель/деферы, std.Thread/io_uring, встроенный фаззер (`testOne(*std.testing.Smith)`).
- **Тулчейн:** бесплатный (ziglang.org), кроссплатформенный — source-backed доступно сразу.
- **Оценка приоритета:** 5 независимых репо подтверждают ценность; пробел полный;
  стоимость source-backed низкая. **HIGH.**

### 3.2 Virtualization / hypervisor

- **Источник:** mohitmishra786 (3 skills: hypervisor-internals 3.5/5, qemu-kvm, containers-internals).
  Содержательно: VT-x VMCS/VMXON/VMLAUNCH, AMD SVM VMCB/VMRUN, EPT/NPT, APICv/Posted
  interrupts, VMEXIT reason-коды, минимальный hypervisor (SimpleVisor/hvpp/kvmm), KVM
  ioctl sketch. Слабости: **без цитат на Intel SDM/AMD APM**, без пининга версий, без
  верификации, монолиты.
- **Рекомендация:** новый домен `virtualization`. Источники — Intel SDM Vol.3C + AMD APM
  Vol.2; верификация — ioctl-демо `/dev/kvm` (`KVM_CREATE_VM`) в QEMU (TCG — работает и на
  Windows-хосте), evals: historical (L1TF/MDS/Spectre-v2 mitigations), adversarial
  (VMEXIT-storm, EPT-violation triage).
- **Приоритет:** 3 скилла подтверждают ценность; пробел полный; QEMU-тулчейн доступен.
  **MEDIUM-HIGH.**

### 3.3 HPC / MPI / RDMA

- **Источник:** mohitmishra786 (3 skills: mpi, openmp, rdma-verbs, все 3/5). Практика
  хорошая (коллективы, non-blocking, Slurm, mpiP/IPM/TAU; libibverbs QP/CQ/MR, RC/UC/UD,
  RoCE vs IB), но примеры не скомпилированы, ссылок на MPI-4.1/OpenMP-spec нет.
- **Рекомендация:** новый домен `hpc`: MPI-4.1, OpenMP 5.x, InfiniBand/RoCE-verb docs, UCX.
  Верификация: `mpicc`+`mpirun` на localhost (Windows: MS-MPI/MPICH) для MPI-семантики;
  RDMA — только документарно (нет оборудования). Исторический eval + adversarial
  (deadlock/ordering).
- **Приоритет:** подтверждено 1 репо (3 скилла); тулчейн MPI доступен, RDMA — нет.
  **MEDIUM.**

### 3.4 OS internals: scheduler/MM/VFS + kernel debugging

- **Источник:** mohitmishra786 `kernel/kernel-internals` — лучший скилл репо (4/5):
  CFS→EEVDF (6.6 → 6.12), vruntime/lag, buddy/SLUB, vmalloc vs kmalloc, VFS dcache/inode,
  page cache, OOM; `kernel-debugging` (kgdb/kdb, ftrace, kprobes, dyndbg, kdump) 3.5/5.
- **Сверка:** TrothByte kernel = RCU/barriers, uaccess, atomic-context. **Scheduler/MM/VFS
  и отладка ядра не покрыты.**
- **Рекомендация:** доработка домена `kernel`: скиллы `kernel-scheduler-mm-vfs` и
  `kernel-debugging`. Источники: docs.kernel.org + исходники `kernel/sched|mm|fs`.
  Верификация: `/proc/*`, ftrace/perf-trace; на Windows-хосте — QEMU+Linux или
  документарно. Пининг ядра ≥6.12.
- **Приоритет:** 1 репо, но глубина/актуальность (EEVDF, SLAB removal) высокая.
  **MEDIUM.**

### 3.5 CAN / automotive protocol RE

- **Источник:** CSS-Electronics/can-bus-reverse-engineering-skills (MIT, 144★, 4/5).
  Сильная сторона — верификация как гейт (`verify.py` exit 0/2, `--selftest`),
  методика «SWEEP для идентификации / HOLDS для калибровки», bias-гейтинг, physical-anchor
  refit, честный fail-path. Слабости: vendor-lock (CANsub-железо), нет J1939/CAN-FD
  edge-cases, нет формального eval-набора.
- **Сверка:** TrothByte `auto-re-protocols-beyond-can` обобщает pipeline
  capture→survey→correlate→bitsearch→verify (и явно ссылается на этот репо как источник
  метода), но **не содержит CAN-специфики**: DBC-формат, cantools, sawtooth bit numbering,
  scale/offset-конвенции.
- **Рекомендация:** доработка `auto-re-protocols-beyond-can` или новый скилл
  `can-signal-extraction` (DBC + cantools + automotive-конвенции). Идеи для переноса:
  dual-excitation, parked-vs-moving гейт, bias-gated rounding, physical-anchor refit.
  MIT — переписывание с атрибуцией допустимо.
- **Приоритет:** 1 репо, пробел частичный. **MEDIUM.**

### 3.6 RDMA / DPU / networking-hardware

- **Источник:** NVIDIA/skills (Apache-2.0 + CC-BY-4.0, 2,945★, **5/5** — единственный
  из всех аудированных, кто соответствует планке TrothByte по верификации): у каждого
  скилла `evals/evals.json` (positive/negative routing-кейсы с assertions) +
  `BENCHMARK.md` 3-Tier отчёт (Security/Correctness/Discoverability/Effectiveness/
  Efficiency, запуск на Claude Code + Codex, uplift vs no-skill), knowledge-map, four-source
  version audit, hardware-safety дисциплина (pre-flight → OOB → window → apply → verify →
  rollback, refuse-and-escalate).
- **Сверка:** TrothByte `networking` = только `ebpf-verifier-reasoning`; RDMA/NIC
  offload/DPU отсутствуют полностью.
- **Рекомендация:** новый скилл `networking-hardware` (RDMA-семантика: QP/MR/atomic ops,
  transport vs link layer; offload/DPDK/eSwitch-концепции). Из NVIDIA взять **только идеи**:
  knowledge-map routing, four-source version audit, hardware-safety change discipline,
  thin-loader + CAPABILITIES/TASKS split. CC-BY-4.0 позволяет переписывать с атрибуцией,
  Apache-2.0 — код.
- **Приоритет:** самый сильный методологический образец; пробел полный. **MEDIUM-HIGH**
  (документарная верификация; RDMA-оборудование недоступно).

### 3.7 Android RE (DEX/smali/ART/JNI)

- **Источник:** SimoneAvogadro (Apache-2.0, 6,793★, 3/5). Важно: это **API-extraction RE**
  (jadx, Kotlin-metadata recovery, fingerprint-first), **не low-level RE** — нет smali,
  DEX-семантики, ART/JIT, JNI/`.so`. Слабые места: нет evals, нет цитат на Kotlin-метадата
  spec/Android docs, `~100% name recovery` — без опубликованных данных.
- **Рекомендация:** новый домен `mobile-android-re`, но нацеленный именно на то, что репо
  опускает: DEX-формат/инструкции, smali, ART internals, R8/ProGuard + Kotlin-metadata
  deobfuscation, нативные `.so`/JNI-анализ. Идеи: fingerprint-first triage, R8-metadata
  name recovery, двухуровневая документация API.
- **Приоритет:** 1 репо с 6,793★ (спрос есть); пробел полный. **MEDIUM** (тулчейн
  jadx/apktool/frida кроссплатформенный).

### 3.8 Shellcode / exploit development

- **Источник:** SnailSploit/Claude-Red (MIT, 2,915★, **2/5**): 58 скиллов/13 категорий, но
  битые ссылки на несуществующие файлы (`templates/harness_min.cc`, `scripts/repro.sh`…),
  CVE-кейсы без цитат (UNVERIFIED), нет evals/CI, монолиты на тысячи строк, keystone-листинг
  содержит мёртвую последовательность инструкций без доказательства исполнения.
- **Рекомендация:** **ПРОПУСТИТЬ как источник контента.** Стратегически это «наступательное
  зеркало» оборонительной миссии TrothByte. Если делать — пересобрать с нуля до планки
  TrothByte (реальный тулчейн, уязвимый таргет, CVE сверены с advisory). Взять как идею:
  crash-triage pipeline (generate → cluster → root-cause → minimize → PoC → reliability-gate
  ≥80%/100 runs), data-only-attacks framing, mitigation matrix.
- **Приоритет:** контент не проходит планку; вопрос миссии. **SKIP / LOW.**

### 3.9 CHERI / capability-архитектура

- **Источник:** qwattash/cheri-skills (BSD-3, 0★, но автор — Alfredo Mazzinghi,
  **исследователь CHERI из Кембриджа** — первичная authority, которой нет ни у кого).
  Покрывает cheribuild, purecap/hybrid ABI, Morello/riscv64, CheriBSD, QEMU-эмуляцию.
  Слабости: 0 evals, нет пининга версий, единственный push (апр 2026).
- **Рекомендация:** новый скилл `cheri-capability-safety`: семантика capability-указателей
  (bounds/tags/PAC-аналоги), purecap-портирование, QEMU-харнесс для CHERI-бинарников.
- **Приоритет:** ценность высокая (капитальная память), конкуренция почти нулевая,
  QEMU-тулчейн есть. **MEDIUM-HIGH** по ценности/effort.

### 3.10 RISC-V / RVV

- **Источник:** alexfdez1010/risc-v-skill (MIT, 2★, свежий, 4/5): VL/AVL strip-mining,
  LMUL/SEW, tail/mask policies, strided/segmented loads, 7 worked kernels, каталог 275 идей
  оптимизаций, явная §Loading Guidance. Слабости: нет evals, claims UNVERIFIED.
- **Сверка:** TrothByte `asm` = x86-only; `simd` = AVX. RVV структурно отличен.
- **Рекомендация:** новый домен `risc-v` (intrinsics + ISA/CSR + QEMU-RVV verification),
  желательно объединить с CHERI (п.3.9) в один домен `risc-v`.
- **Приоритет:** пробел полный, тулчейн clang+QEMU RVV доступен. **MEDIUM-HIGH.**

### 3.11 FPGA / HDL

- **Источник:** LNC0831/oh-my-fpga (MIT, 12★, 4/5): 13–14 скиллов (timing-closure, cdc-audit,
  constraints, lint-triage, coverage-closure, sim-bringup, zynq-bringup). Правильные гейты
  («CDC constraint не чинит CDC bug», «никогда не делай число зелёным, скрывая violation»,
  classify-before-fix). Слабости: верификация завязана на проприетарный Vivado+MCP.
- **Рекомендация:** новый домен `hdl` с **OSS-верификацией** (yosys/nextpnr/Verilator/Icarus)
  вместо Vivado-зависимости. Идеи: CDC-классификация, «constraint vs structure» правило.
- **Приоритет:** ниша без конкуренции; OSS-тулчейн кроссплатформенный. **MEDIUM.**

### 3.12 Fuzzing-харнесы (расширение sanitizers)

- **Источник:** 0xazanul/fuzz-skill (43★, нет лицензии, 3.5/5) — лучший пример
  evidence-дисциплины: «Proof Standard» (нет утверждения о CVE без воспроизводимого
  sanitizer-отчёта + минимизированного инпута + достижимого пути), «нет находок — только с
  coverage/runtime/limitations», target-map-first.
- **Рекомендация:** доработка домена `sanitizers`: скилл `fuzzing-harness-evidence-gate`
  (AFL++/libFuzzer + sanitizers, proof standard, crash minimization, sibling-bug search).
  Пересекается с `sanitizer-report-reading` и историческим eval-материалом из
  `research/2026-08-15-agent-failures-survey.md`.
- **Приоритет:** тулчейн доступен; пробел частичный. **HIGH** по соотношению effort/ценность.

### 3.13 UEFI / firmware (расширение bootloader)

- **Источник:** MarsDoge/uefi-firmware-skill (MIT, 1★, 3/5): UEFI/PI spec-boundary, edk2,
  HII/VFR, ACPI/SMBIOS, Secure Boot, QEMU serial-log debugging. Слабости: нет evals.
- **Сверка:** `bootloader` TrothByte — OS-bootloader; UEFI/BIOS-слой не покрыт.
- **Рекомендация:** доработка `bootloader` → скилл `uefi-firmware` (spec-boundary
  дисциплина, edk2, QEMU-верификация).
- **Приоритет:** 1 репо, ниша без конкуренции. **LOW-MEDIUM.**

---

## 4. Подробный разбор слабых мест в пересекающихся доменах

### 4.1 Верификация — где внешние сильнее TrothByte (архитектурный урок)

| Критерий | TrothByte | trailofbits/skills | wshobson/agents |
|---|---|---|---|
| Eval-инфраструктура | 4 категории, но не исполняются CI | `case.yaml`+fixtures+weighted-graders, **ablation Δ** (with-plugin минус baseline), positive+negative пары | PluginEval: static (7 субчеков) + LLM-judge (4 измерения) + **Monte-Carlo активация 50–100 прогонов** |
| Статистика | нет | Δ per case | **Wilson/Clopper-Pearson CI, Cohen's κ, Elo-ранжирование корпуса**, letter grades, Bronze–Platinum |
| Проверка валидатора | skill_lint/registry_check | validator `--self-test` (известно-плохой плагин обязан провалиться; мало assertions = fail), **>500-строчные SKILL.md = warning**, версионный bump в CI | `make garden` drift-detection (генерация пяти харнессов из одного источника, расхождение = fail) |
| Реальный тулчейн в CI | разовые прогоны | cross-compile C→aarch64, semgrep-грейдинг, llvm-19, pinned CLIs (Claude Code 2.1.220, Codex 0.146.0) | real-CLI round-trips (OpenCode 1.1.23: 191/191; поймано 2 реальных бага адаптеров) |

**Что взять в TrothByte (идеи):**
1. **Ablation-Δ и FP-verdict-таксономия** (TRUE_POSITIVE/LIKELY_TP/LIKELY_FP/FALSE_POSITIVE/
   OUT_OF_SCOPE) — прямо ложится на FP-evals TrothByte.
2. **Monte-Carlo активация скиллов** — прямое измерение для routing-evals.
3. **Coverage-gate-файлы** («каждый назначенный bug-class обязан быть `filed:` или
   `cleared <seed phrase>`, `skipped:` невалиден») — усиливает правило «never silently mark
   complete».
4. **`--self-test` для валидаторов** — чекер, который ничего не ловит, обязан падать.
5. **«Находки обмениваются файлами на диске, а не текстом в ответе»** — контекстная
   экономия при многопоточном запуске.
6. **Real-CLI round-trip перепроверка source-backed claims** (скриптованные прогоны
   gcc/rustc/clang вместо разовых disasm) — делает source-backed устойчивым.

### 4.2 Прямой вызов методологии TrothByte (trailofbits)

trailofbits AGENTS.md содержит правило: **«Не добавляйте verification-скэффолдинг в промпты
("Double-check your answer") — это ухудшает вывод; положите проверку в `make check` или
валидатор, где она выполняется детерминированно».** Это прямо противоречит подходу
TrothByte, где 6 meta-скиллов (evidence/verification/completion) — это prompt-level
скэффолдинг по дизайну.

**Резолюция для репозитория:** meta-скиллы руководят **суждением** (когда сомневаться,
какие evidence-маркеры ставить), но **единственный реальный гейт** — это `tools/`
валидаторы + evals. Формулировку этой дихотомии стоит явно зафиксировать в
`meta-verification`/docs/architecture.md.

### 4.3 Другие пересекающиеся слабости внешних

- **Jeffallan/claude-skills (MIT, 11,014★):** валидатор проверяет форму (frontmatter, длина,
  ссылки), а не суть; разовый LLM-аудит нашёл **30+ phantom-ссылок в related-skills**,
  которые валидатор пропустил → урок: нужна **проверка существования related-skills для
  `cross-links.yaml`** (TrothByte может сделать это детерминированно в registry_check).
- **P4nda0s/reverse-skills:** монолиты без references; но negative-guidance с обоснованием
  («Do Not Blindly Hook init») — готовая пища для `meta-rationalizations`.
- **Masriyan (MIT, 335★):** chaining-таблицы «условие → следующий скилл» — конкретный
  механизм routing; output-template-first — совпадает с evidence-дисциплиной TrothByte.
- **mohitmishra786:** schema-drift (часть скиллов без `Triggers`, хотя AGENTS.md требует) —
  та же проблема, которую TrothByte уже решает через skill_lint.

---

## 5. Лицензионная сводка

| Лицензия | Репозитории | Условия заимствования |
|---|---|---|
| **MIT** | wshobson/agents, Jeffallan/claude-skills, Masriyan, CSS-Electronics, SnailSploit, nzrsky/zig-skills, whit3rabbit, mohitmishra786, alexfdez1010, LNC0831/oh-my-fpga, MarsDoge, hackersifu | Текст можно заимствовать **с атрибуцией и сохранением copyright-нотиса**; для TrothByte (MIT-репо) совместимо |
| **Apache-2.0** | NVIDIA/skills (код), SimoneAvogadro, mukul975, rekit | Текст/код с атрибуцией, нота о патентах; совместимо с MIT при сохранении нотиса |
| **CC-BY-4.0** | NVIDIA/skills (документация/скиллы) | Переписывание с атрибуцией; **не-share-alike** — совместимо |
| **CC-BY-SA-4.0** | trailofbits/skills | **Share-alike (viral):** переписывание с атрибуцией, но производное должно распространяться под CC-BY-SA — в MIT-репо **нельзя** переносить формулировки напрямую; только идеи/структура |
| **BSD-3-Clause** | qwattash/cheri-skills | С атрибуцией |
| **Нет лицензии** | rudedogg, zigcc, P4nda0s (заявлен MIT, файла нет), cellebrite-labs/ghidra-rpc, manyuegong33/r0crawl_skills, 0xazanul/fuzz-skill, n132/*, ajul8866/momo, libfuzzer-pipeline, sector-b79, headless-ghidra | **Заимствовать текст нельзя.** Только идеи/структура/методика. Перед любым копированием — запросить разрешение автора |

**Практическое правило для репозитория:** формулировки переносятся только из MIT/Apache/
BSD/CC-BY (с атрибуцией в docs/ACKNOWLEDGMENTS.md); из CC-BY-SA и безлицензионных — только
идеи, полностью переписанные и перепроверенные по первичным источникам.

---

## 6. Что не удалось проверить

- **Z3Prover/z3** — это проект SMT-солвера, а не skill-репозиторий; в исходном списке 18
  он значится, но к аудиту навыков не относится. Формальный/символический анализ (z3-API
  как навык) — отдельный потенциальный домен, не подтверждён ни одним skill-репо.
- **Лицензия P4nda0s/reverse-skills**: GitHub API возвращает `null`; README заявляет MIT,
  но LICENSE-файла в дереве нет → статус UNVERIFIED.
- **trailofbits/skills evals**: подтверждено, что `evals/` существуют и запускаются
  командой `claude plugin eval . --ablation with-without --judge-model opus`, но CI-гейта
  на evals не найдено (запуск ручной) — в т.ч. поэтому «Δ-насыщение» и «skill не срабатывает»
  признаются самими авторами.
- **wshobson/agents**: round-trip для Cursor-харнесса не верифицирован реальным CLI
  (только рецепт); coverage честно ограничен («нет проверки потребления модели без
  трат на API»).
- **whit3rabbit/claude-zig-skill**: заявление о «223 tested recipes против 0.15.2»
  унаследовано из отдельного репо (zig-bbq-cookbook) и **внутри репо не воспроизводимо**;
  упомянутый в README `docs-test/` в дереве отсутствует.
- **SnailSploit/Claude-Red**: CVE-кейсы (CVE-2025-0910, CVE-2024-4852, CVE-2025-20301,
  CVE-2024-4455, CVE-2024-7971) заявлены без цитат; часть файлов-артефактов (harness_min.cc,
  repro.sh, record_rr.py, va_aliases.txt) в репо не существует.
- **mohitmishra786**: заявленные «26 категорий» — по факту **25** каталогов в дереве
  (README насчитывает 143 скилла при 142 в дереве; category «Runtime Safety» в README = 6,
  в дереве = 5). Принято 142/25.
- **Детали нишевых репо из субагента E** (r0crawl 196 модулей, ghidra-rpc, oh-my-fpga,
  cheri-skills, risc-v-skill и др.): просмотрены README + 1–2 SKILL.md + LICENSE; полное
  содержимое всех модулей не перечитывалось. Глубина некоторых (напр., n132-эксплойт-техники)
  оценена по структуре/README + ключевым файлам.
- **LinkedIn** в этот аудит не входил (отсутствовал в задании); X/Twitter и полнотекстовые
  поисковики (DDG/Bing/Google) блокируют ботов — поиск новых репо велся через `gh search`
  и каталоги.
- **Верификация claim'ов внутри внешних репо** (например, «~100% Kotlin-name recovery» у
  SimoneAvogadro) не выполнялась независимыми прогонами тулчейна — только проверка наличия
  методики/файлов.

---

## 7. Итоговые рекомендации по дорожной карте

**Новые домены (в порядке приоритета):**
1. `zig` — пробел подтверждён 5 репо; тулчейн бесплатный; дифференциация «единственный
   verified+evaled+version-pinned набор Zig-скиллов». (HIGH)
2. `virtualization` — контент-основа есть (mohitmishra), SDM/APM первичка, QEMU-верификация. (MEDIUM-HIGH)
3. `risc-v` (+CHERI) — конкуренция почти нулевая, QEMU-RVV/CHERI-тулчейн. (MEDIUM-HIGH)
4. `hpc` (MPI/RDMA) — MPI верифицируем локально; RDMA документарно. (MEDIUM)
5. `mobile-android-re` — низкоуровневая часть (DEX/smali/ART/JNI), а не API-extraction. (MEDIUM)
6. `hdl` — с OSS-верификацией (Verilator/yosys). (MEDIUM)

**Доработки существующих доменов:**
- `kernel` → scheduler/MM/VFS + kernel-debugging (ftrace/kgdb/kdump).
- `sanitizers` → `fuzzing-harness-evidence-gate` (AFL++/libFuzzer + Proof Standard).
- `reverse-engineering` → `can-signal-extraction` (DBC/cantools/automotive) внутри
  auto-re-protocols-beyond-can.
- `bootloader` → UEFI/BIOS firmware-слой.
- `networking` → RDMA/offload-концепции (методология NVIDIA как образец).
- `assembly/abi` → CPU-архитектура (pipeline/speculation/TLB/MESI) как references.

**Методологический перенос (архитектура скиллов):**
- ablation-Δ + FP-verdict-таксономия (trailofbits) → FP-evals;
- Monte-Carlo активация + статистические CI (wshobson) → routing-evals;
- coverage-gate-файлы (trailofbits) → усиление «never silently complete»;
- `--self-test` валидаторов и проверка существования related-skills в `cross-links.yaml`;
- зафиксировать дихотомию «meta-скиллы руководят суждением, гейт — валидаторы/evals» в
  docs/architecture.md (ответ на прямой вызов trailofbits).

**Пропустить:** контент SnailSploit/Claude-Red (не проходит планку верификации); копирование
текста из CC-BY-SA (trailofbits) и безлицензионных репо — только идеи.
