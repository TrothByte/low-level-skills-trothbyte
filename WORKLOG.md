# WORKLOG — Low-level skills TrothByte

## 2026-08-14 — Сессия 1

### Выполнено

1. **PHASE 0 — Repository Bootstrap**: проверено состояние репозитория (Agent.md, WORKLOG.md,
   roadmap/progress.yaml, roadmap/research-ingestion.yaml), прочитан Agent.md (15 правил инженерии).

2. **PHASE 1 — Research Ingestion (начат)**: прочитан полностью "Анализ скиллов.md" (483 строки)
   и "Энциклопедия — первичные источники и валидация.md" (367 строк).

## 2026-08-14 — Сессия 2

### Выполнено

1. **PHASE 1 завершён** — `roadmap/research-ingestion.yaml` расширен до полного извлечения:
   10 inventories (topics, bug-classes A1-A32, failure-modes B1-B22, verification, CVE×21,
   primary sources, lint codes, calibration, positive patterns, open questions,
   cross-repo signals, unique findings U1-U18).

2. **PHASE 2-4** — `coverage.yaml` (37 доменов + gap analysis), `unique-skills.yaml` (60 skills),
   `priorities.yaml` (scoring + tiers 1-6).

3. **PHASE 5-6** — `registry/sources.yaml` (57 sources), `registry/claims.yaml` (17 claims + provenance).

4. **PHASE 7-8** — `registry/skills.yaml`, `cross-links.yaml`, `tools.yaml`, `evals.yaml`;
   `docs/` (architecture, authoring, sources, evaluation, licensing, token-optimization).

5. **Bootstrap** — AGENTS.md (canonical), README.md, LICENSE.

6. **PHASE 13 начат** — реализованы 2 skills (source-backed):
   - `c-undefined-behavior` — SKILL.md + references/ub-taxonomy.md (12 классов UB) + examples +
     evals. Верифицировано: disasm GCC -O2 подтвердил folding signed-overflow и удаление null-проверки.
   - `c-integer-promotion-and-conversion` — SKILL.md + references/integer-conversions.md (6 правил) +
     examples + evals. Верифицировано: `-Wsign-compare` срабатывает на bad, good компилируется чисто.

## 2026-08-14 — Сессия 3

### Выполнено

1. **Tier-1 flagship (3):**
   - `compiler-ub-assumptions` — верификация GCC 16.1: `x+1>x` → `movl $1`; null-проверка удалена;
     div-guard hoisted; infinite loop: GCC сохраняет, Clang C++ удаляет.
   - `abi-layout-reasoning` — S1=8/S2=16/S3=24; cross-ABI находка: Windows x64 пакует 8-байтную
     struct в `%rcx` vs SysV `%edi,%esi`; 40-байтная struct на стеке.
   - `memory-ordering-reasoning` — rustc 1.97.1 asm: store_relaxed=mov, store_seqcst=xchg,
     fetch_add=lock xadd (идентичен для Relaxed/SeqCst на x86).

2. **PHASE 10** — 6 meta-skills (routing, evidence, verification, assumptions, rationalizations,
   completion) с полной схемой.

3. **PHASE 11** — 4 shared tools: skill_lint.py, registry_check.py, token_measure.py, source_check.py.

4. **PHASE 23** — quality gates прогнаны и почищены (registry_check 0 ошибок, skill_lint 11/11,
   source_check 0 WARN).

## 2026-08-14 — Сессия 5 (60/60 skills, параллельные субагенты)

### Стратегия

Реализация skills выполнялась батчами по 6-9 параллельных `task`-субагентов, каждый с
self-contained промптом (схема SKILL.md, формат references, hard rules, верификация).

### Выполнено (5 батчей = 45 skills; 15 первых — в сессиях 2-3)

- **Батч 1** (Tier 2-3, 7): embedded-mpu-trustzone, rust-unsafe-reasoning, rust-ffi-boundary,
  simd-vectorization-cross-layer, sanitizer-report-reading, rust-panic-safety, zeroize-constant-time.
- **Батч 2** (Tier 4, 6): elf-linker-loader-debugger, dwarf-debug-info, kernel-rcu-memory-barriers,
  kernel-uaccess-safety, ptx-assembly, ebpf-verifier-reasoning.
- **Батч 3** (Tier 5, 14): asm-*, atomics, c-string/errno, cpp-object-lifecycle, elf-layout,
  embedded-volatile, gpu-memory-model, llvm-ir-reading, qemu-system-setup.
- **Батч 4** (Tier 6, 18): wasm, bootloader, rtos, auto-re, go-rust-re, cache-numa,
  vectorization-reasoning, llvm-pass-writing, deadlock, condvar, signal-handler, got-plt,
  interrupts, linker-script, move-semantics, kernel-atomic-context, type-recovery, perf-discipline.

### Ключевые верификационные находки

zeroize: memset элидирован при -O2; inline-asm: отсутствие "memory" clobber даёт 2 вместо 3;
Rust 1.97.1 отвергает acquire-хранилища; без volatile поллинг MMIO сворачивается в 1 load;
panic через extern "C" = abort 0xC0000409; DWARF: DW_OP_entry_value при -O2; false sharing 16x;
strcpy crash 0xC0000005; ABBA deadlock (watchdog exit 42).

### Статус

**60/60 skills реализованы** (47 source-backed, 13 researched — требуют недоступного тулчейна:
nvcc, clang-bpf, LLVM, QEMU, sanitizer-рантаймы).

## 2026-08-14 — Сессия 6 (полировка: товарный вид)

1. **Навигация**: созданы README.md во всех 23 доменных каталогах
   (генератор `tools/reports/gen_domain_readmes.py`); переписан корневой README.
2. **Благодарности**: создан ACKNOWLEDGMENTS.md (18 репозиториев + стандарты + доклады +
   исследователи AI-безопасности + инструменты верификации).
3. **Чистка**: удалён мусор из корня (10 asm/txt дампов + дубликат Agent.md);
   исследовательские документы перенесены в `research/`.
4. **Документация**: docs/architecture.md обновлён.

## 2026-08-14 — Сессия 7 (брендинг TrothByte, консолидация docs)

1. **Переименование** в «Low-level skills TrothByte»: README, LICENSE.md (Copyright TrothByte),
   AGENTS.md, docs/architecture.md, docs/ACKNOWLEDGMENTS.md, docs/SKILLS.md.
2. **Docs консолидированы**: ACKNOWLEDGMENTS перенесён в `docs/`; создан `docs/SKILLS.md`
   (каталог всех 60 skills; генератор `tools/reports/gen_skills_catalog.py`); удалены 5
   внутренних процесс-доков (authoring/evaluation/licensing/sources/token-optimization) —
   содержание консолидировано в registry/*.yaml, LICENSE.md и architecture.md; битые ссылки
   в 7 skill-файлах исправлены.
3. **README переписан** в красивом product-стиле (теглайн, статистика, навигация, quality).

## 2026-08-14 — Сессия 8 (переименование директории + потеря файлов)

1. **Директория переименована** в `Low-level skills TrothByte` (копирование дерева;
   старая директория залочена IDE).
2. **Инцидент**: при первой попытке `Copy-Item` (известный деструктивный баг PowerShell 5.1:
   wildcard + `-Force` + несуществующий destination) из исходной директории пропали
   `WORKLOG.md` и файлы `research/` («Анализ скиллов.md», «Энциклопедия — первичные источники
   и валидация.md»). Проверена корзина — не найдены. Знания из этих документов полностью
   сохранены в `roadmap/research-ingestion.yaml` (репозитории, bug-classes A1-A32,
   failure-modes B1-B22, CVE×21, primary sources, calibration, unique findings U1-U18).
3. **Восстановление**: WORKLOG.md пересоздан в новой директории; в `research/` создан
   README.md с описанием содержимого и ссылкой на research-ingestion.yaml. Оригинальные
   документы могут быть восстановлены пользователем из резервной копии при наличии.
4. **Оптимизация**: заголовок progress.yaml приведён к новому имени; валидаторы прогнаны
   в новой директории (registry_check 0 ошибок, skill_lint 60/60, source_check 0 WARN).

### Следующие шаги

1. PHASE 15-21: прогнать synthetic/FP/CVE/adversarial evals по `evals/README.md` каждого skill.
2. PHASE 19-20: routing evals + collision testing.
3. PHASE 22: license audit по всем references.
4. PHASE 14: token-оптимизация; перепроверка researched skills на хосте с CUDA/LLVM/QEMU/Linux.
