# Survey: отказы и галлюцинации ИИ-агентов в ассемблере и ассемблерной разработке

**Дата:** 2026-08-15 · **Статус:** исходная аналитика для доменов `assembly`/`binary-analysis`
и новых скиллов · **Репозиторий:** TrothByte/low-level-skills-trothbyte

---

## 1. Дата и метод исследования

**Дата проведения:** 2026-08-15.

**Метод.** Все 5 субагентов были сфокусированы исключительно на ассемблере и смежных
задачах (инструкции, opcodes, регистры, inline asm, calling conventions, дизассемблирование,
декомпиляция, shellcode, LLM-as-compiler). Каждый обязан был fetch'ить источники
(arxiv API/abs, old.reddit, `gh`/GitHub API, HN Algolia, блоги), а не пересказывать.

| Субагент | Канал | Особый фокус |
|---|---|---|
| A | Академия (arxiv API) | LLM-as-compiler, дизассемблер, декомпиляция, RE-бенчмарки |
| B | Reddit (old.reddit) | Галлюцинации инструкций/регистров в r/asm, r/RISCV, r/ClaudeAI |
| C | GitHub (`gh` API) | PR/issue с AI-ассемблером (HerraduraKEx, BBoeOS, System1, cbm-filebrowser, ocrosby, claude-code) |
| D | HN (Algolia) + блоги | Meta LLM Compiler, Lemire NEON, C64 Gemini, Reflex |
| E | RE-сообщества + академия | Shellcode-интерпретация, Ghidra-MCP, деобфускация, Codex-vs-Claude |

**Точечно верифицированы лично (abs-страницы arxiv):** NeuComBack (2511.01183),
SuperCoder (2505.11480), FidelityGPT/DeGPT (2510.19615), Deconstructing Obfuscation
(2505.19887). Ранее верифицированные: CompilerEval (2511.04132), Meta LLM Compiler
(2407.02524), LLM4Decompile (2403.05286), REx86 (2510.20975).

**Ограничения:**
- Semantic Scholar API вернул 403 (rate-limit) — не использовался.
- arXiv: **0 статей** по `LLM + shellcode` и `LLM + calling convention` — явный
  литературный пробел (см. §8).
- **Бенчмарка, который считал бы именно частоту ошибок на уровне mnemonic/opcode/operand,
  в литературе не найдено** — ближайшие суррогаты: функциональная корректность
  (NeuComBack, SuperCoder), error-анализ peephole (2412.12163), cliff-эффект на 200
  инструкций (2607.06125).
- Reddit/форумы: DDG-капча, r/ExperiencedDevs/r/rust/r/cpp/r/reverseengineering —
  специфичных ассемблерных тредов с конкретикой не дали (честно помечено субагентом B).
- Часть выводов (ArchCloudLabs, Quesma, joshuamckiddy) — одиночные кейсы на основе
  реальных транскриптов; воспроизводимость не проверялась.

---

## 2. Сводная таблица

### 2.1 Галлюцинации инструкций / регистров / синтаксиса (главная больная тема)

| ID | Источник | Механизм | Покрытие TrothByte | Рекомендация |
|---|---|---|---|---|
| ASM-1 | r/asm 14q5qi8 (2023) | AT&T `movl 0x0, -0x4(%rbp)`: перепутаны source/dest, `0x0` принят за immediate, а не memory-операнд | ◐ asm-x86-64 (адресация) | **E** references + **EV** |
| ASM-2 | r/asm 13ws91e (2023) | «AX — 8-битный аккумулятор»; «фикс» идентичен оригиналу `mov [charcount], ax` | ✅ asm-x86-64-registers | **EV** |
| ASM-3 | r/asm 1lkb4uj (~2025) | Copilot сфабриковал CDC COMPASS-программу: выдуманные pseudo-ops (`JOB`, `SST`, `OCT`), Return-Jump на несуществующую `PRTSTR`, самопереход | ✖ | **N** asm-verification |
| ASM-4 | r/asm 1cw2loj (2024) | ChatGPT 3.5: `sys_read` «строка»→число, конверсия молча теряет разряд (`54`→`6`) | ◐ asm-signed-unsigned | **EV** |
| ASM-5 | r/asm 152w2uj (2023) | AES-NI NASM: `decrypt(encrypt(x)) ≠ x` — сгенерированный key schedule/инструкции не round-trip'ят | ✖ | **EV** historical |
| ASM-6 | r/asm 1ewzfz6 (2024) | DOS→long-mode: identity-map «times 512 dq 0x3» (все entry указывают на одну область), перезагрузка системы; DOS-строки `$`-terminated приняты за null-terminated | ◐ bootloader-stages | **EV** historical + **E** |
| ASM-7 | r/RISCV 14pde9k (2023) | RISC-V рекурсия: `s0` не инициализирован, кадр 4 байта вместо 8 (s0+ra), сумма — мусор | ✅ asm-calling-conventions | **EV** |
| ASM-8 | GitHub HerraduraKEx PR#33 (2026, MERGED) | NASM i386: `mov ecx,[esp+4]` вместо `[esp]` — неверный стек-офсет, «продукты пишутся не в тот слот»; ARM Thumb-2: `cbz r9/r10` — невалиден для hi-registers (только r0–r7) | ◐ | **EV** historical |
| ASM-9 | GitHub BBoeOS PR#506 (2026, MERGED) | NASM 3.x: `global $abs`/`extern $abs` — `$` недопустим в аргументах директив (работал только в 2.x) | ✖ | **EV** + **E** NASM-диалект |
| ASM-10 | GitHub BBoeOS PR#550 (2026, MERGED) | Inline asm в object-mode: ссылки на `_g_<name>`-символы, nasm трактует undefined как 0 → каскад «label changed during code generation» | ◐ asm-inline-asm | **EV** |
| ASM-11 | GitHub BBoeOS PR#584 (2026, MERGED) | `imul eax, eax, 38`: парсер принял второй регистр за часть immediate → обнулённый immediate; плюс truncation в emit_word | ✖ | **EV** historical |
| ASM-12 | GitHub ocrosby PR#33 (2026, MERGED) | 4 задокументированных класса NASM-ошибок: case-sensitivity (`Loop`≠`loop`≠`LOOP`), `mov rax,buf` vs `mov rax,[buf]` (адрес vs содержимое, «не ошибка — просто не то»), missing size hints (`inc qword [counter]`), missing `default rel` (Mach-O) | ✖ | **N**/E asm-syntax-dialects |
| ASM-13 | GitHub System1 PR#1 (2026, OPEN) | Bootloader копирует весь образ 1.44 MB в 0x00200000 при `-m 1M` QEMU → чтение BPB из unmapped памяти → `panic("Unable to mount FS")` | ◐ bootloader | **EV** |
| ASM-14 | GitHub cbm-filebrowser PR#1 (2026, OPEN) | 6502: цикл `parseext` шагает по 4-байтным слотам, но таблица не кратна 4 (`"tcrt"` = 5) → `beq` вместо `bcs`, бесконечный цикл | ✖ | **EV** |
| ASM-15 | GitHub claude-code #52688 (2026) | AUP false-positive: Claude Code блокирует легальную работу с HLASM/z-архитектура на zowe-common-c | — инструмент | пропустить |
| ASM-16 | GitHub box64 #4214 (2026) | Claude Code-ревью dynarec-ассемблера: double-free, неверные 32/64-bit размеры — находки не верифицированы автором/мейнтейнером | ◐ meta-evidence | **EV** FP |
| ASM-17 | GitHub MintVID PR#67/71 (2026, MERGED) | m68k hand-asm: байт-ордерный баг найден только дифференциальной верификацией; `ld --wrap`-мисасumption пойман `qemu -d exec` (код никогда не исполнялся) | ◐ meta-verification | **EV** |
| ASM-18 | Lemire, lemire.me (2026) | NEON-SIMD циклы: пропущен guard переполнения счётчиков по lane (нужен horizontal reduce каждые 255 итераций); 1200→154 инструкций (8×) | ✖ (asm — x86-only) | **N**/E + **EV** |
| ASM-19 | C64 Gemini (Medium/HN, 2025) | 6502 `*40`: `asl`/`rol` теряют выдвинутые биты (10-битный результат) | ✖ | **EV** |
| ASM-20 | ilbertt/reflex (HN, 2026) | Модель эмитит raw CHIP-8 opcodes: «two plus three» → неверные операнды (cosine sim «two»vs«2» = 0.09) | ✖ | идея |
| ASM-21 | r/ClaudeAI (2026), JetBrains-инженер | Claude Code «уверенно выдумывает то, что не может прочитать из байтов»: куда резолвится `call_indirect`, LEB128-индексы, кадры краша — «wrong and invisible» | ✖ | **N** asm-verification |

### 2.2 Дизассемблирование / декомпиляция (plausible-but-wrong)

| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| DEC-1 | Meta LLM Compiler (2407.02524) | Дизассемблер asm→LLVM-IR: **45% round-trip, 14% exact match** | ◐ binary-analysis | **E** калибровка |
| DEC-2 | LLM4Decompile (2403.05286) | Декомпиляция→C по re-executability: GPT-4o побеждён на >100%; fine-tuned V2 до 64.9% | ◐ | **E** |
| DEC-3 | FidelityGPT/DeGPT (2510.19615, NDSS'26) | «Починка» декомпиляций: DeGPT FR 83% / **CFR 37%** — ~63% «исправленных» строк остаются неверными | ✖ | **E** + калибровка |
| DEC-4 | SLaDe (2305.12520) | Нейродекомпиляция «обычно некорректна» (читаема, но неверна); 4× точнее ChatGPT, 6× Ghidra | ◐ | **E** |
| DEC-5 | CoDe-R (2604.12913) | Назван центральный дефект: «logical hallucinations» / «semantic misalignment» при декомпиляции | ◐ | **E** |
| DEC-6 | SCDBench (2605.29059) | EVM-байткод: «выглядит компилируемо и правдоподобно, семантика расходится»; идеал — 7% (42/600) | ✖ | **E** |
| DEC-7 | Context-Guided (2511.01763) | Декомпилированный код «обычно лишь семантически правдоподобен, а не исполняем» | ◐ | **E** |
| DEC-8 | 2607.06125 (TOSEM) | **Capability cliff на ~200 инструкциях**; метрики расходятся: compile@k5 до 79.4%, pass@k сильно ниже («выглядит правильно, но не работает») | ◐ | **E** + калибровка |
| DEC-9 | REx86 (2510.20975, ACSAC'25) | Базовые LLM галлюцинируют комментарии к x86-дизассемблеру; fine-tuning снижает (solve 31%→53%) | ◐ | **E** |
| DEC-10 | ArchCloudLabs shellcode_gpt (2023) | GPT-3 разбор 111-байтного bind_tcp shellcode: bind назван reverse, неверные syscalls, выдуманная инструкция, не извлечён IP/порт (`0x7f000001`/`0x5c110002`) | ✖ | **N**/E shellcode |
| DEC-11 | Quesma Ghidra-MCP (2026) | Claude на 6502-ROM: уверенно «Centipede» (на самом деле River Raid); не смог rebase ($0000 vs $A000) и записать байт-патч (`DEY`→`NOP` сделал человек) | ◐ binary-analysis | **EV** FP |
| DEC-12 | Codex vs Claude, Cerber5.exe (2026) | Противоположные «high confidence» вердикты об упаковке: Claude «packed», Codex «not packed» (FLOSS/capa молча заблокированы sandbox) | ◐ meta-evidence | **EV** FP |
| DEC-13 | Deconstructing Obfuscation (2505.19887) | 8 моделей на OLLVM-x86-64: 5 классов ошибок (predicate misinterpretation, structural mapping, control-flow, arithmetic, constant propagation); **универсальный провал на комбинированной обфускации**; конкретика: GPT-4o `(input^0xe6c98769)*((input&2)|2)` вместо `(input|0xBAAAD0BF)*(input^2)` | ✖ | **EV** adversarial |

### 2.3 LLM-as-compiler (генерация ассемблера из кода/IR)

| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| LAC-1 | CompilerEval (2511.04132) | LLM как end-to-end компилятор: «низкий процент успешной компиляции»; ошибки разобраны в теле статьи | ◐ asm/compiler | **E** references |
| LAC-2 | NeuComBack (2511.01183, NeurIPS'25) | IR→asm: базовая функциональная корректность **44% (x86_64) / 36% (aarch64)**; self-evolving prompt → 64%/58%; 14/16 программ быстрее clang -O3 | ✖ | **E** калибровка |
| LAC-3 | SuperCoder (2505.11480) | Ассемблер-супероптимизация: Claude-opus-4 **51.5% test-passing** на 8,072 программах; RL (с 61.4%) → **95.0%** | ✖ | **E** калибровка |
| LAC-4 | CISC→RISC (2411.16341) | Транспиляция x86→ARMv5 **79.25%**, x86→RISC-V **88.68%** | ✖ | **E** |
| LAC-5 | Peephole (2412.12163) | Ошибки на уровне отдельных инструкций при одной peephole-оптимизации AArch64 (регистры/операнды); CoT даёт преимущество | ◐ llvm/asm | **E** |
| LAC-6 | BinMetric (2505.07360) | 1000 вопросов, 6 задач; **assembly synthesis / binary lifting — слабые задачи** LLM | ◐ | **E** калибровка |

### 2.4 RE / агентные бенчмарки и противовесы

| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| REB-1 | SRE-Bench (2608.11469) | Агентный RE-бенчмарк: сильнейшая модель **61.4% per-instance, 31.5% полных решений**; «сильные source-навыки не переносятся на binary» | ◐ binary-analysis | **E** калибровка |
| REB-2 | Binaries Talk Back (2607.12507) | Адверсариальные бинарники: без контроля модели предлагают unsafe-действие **35/40** против **0/40** на чистых; provenance-gating режет ложные валидации 32/32→0/32 | ✖ | **EV** adversarial |
| CTR-1 | Sean Heelan (2026) | Контрпример: Opus 4.5/GPT-5.2 собрали 40+ рабочих эксплойтов для QuickJS 0-day (RELRO+CFI+shadow-stack+seccomp bypass) | — | контекст/калибровка |

### 2.5 Структурный пробел бенчмарков

| ID | Источник | Механизм | Рекомендация |
|---|---|---|---|
| BEN-1 | r/asm 17r0gs1 (2023), Continue.dev | Ассемблер отсутствует во всех мультиязычных бенчмарках: MultiPL-E (19), BabelCode (16), MBXP (13), HumanEval-X (5); в The Stack всего 2.36 GB asm; нет в AlphaCode/CodeGen/PolyCoder → галлюцинации mnemonics непоймаемы ни одним бенчмарком | **E** (обоснование уникальности) |
| BEN-2 | Субагент A (2026) | **В литературе нет бенчмарка, считающего частоту ошибок уровня mnemonic/opcode/operand** — ближайшие суррогаты LAC-2/LAC-3/DEC-8 | **N** (позиционирование) |

---

## 3. Галлюцинации инструкций — углублённо (приоритет)

Это самая больная тема ИИ-программирования, и она же наименее измерена. Классы:

1. **Выдуманные mnemonics/pseudo-ops** (ASM-3): Copilot выдал `JOB`/`SST`/`OCT` в CDC
   COMPASS — программу, которая «выглядит как ассемблер», но не собирается. Класс
   «изобретённая инструкция» подтверждается и GPT-3-shellcode-кейсом (DEC-10,
   «decrements the AL register» — инструкции в коде нет).
2. **Неверные операнды/ширины/офсеты** (ASM-1, ASM-2, ASM-8, ASM-11): инверсия
   source/dest в AT&T, «AX = 8 бит», `[esp+4]` вместо `[esp]`, `imul eax,eax,38`
   с обнулённым immediate. **Характерная черта: молчаливая порча данных без ошибки**
   (ASM-4: `54`→`6`; ASM-5: AES round-trip не сходится) — «не ошибка, просто не то».
3. **Ограничения ISA/диалекта** (ASM-8b, ASM-9, ASM-12): Thumb-2 `cbz` только для
   r0–r7, `$` недопустим в NASM-директивах 3.x, case-sensitivity, `mov rax,buf`
   vs `[buf]`, size hints, `default rel`. Класс подтверждён 4 независимыми правилами
   из ocrosby PR#33.
4. **Байт-уровневая слепота** (ASM-21, DEC-10, DEC-11): модель «не может прочитать
   байты», но уверенно выдумывает факты (call_indirect-резолв, LEB128, краш-кадры,
   IP/порт из констант). Именно поэтому нужен **парсер/тулчейн как источник истины**.
5. **Битовая арифметика** (ASM-19): ASL/ROL теряют выдвинутые биты при `*40` на 6502.

**Вывод:** ни один внешний репозиторий (из аудита 18 репо) не содержит скилла
«проверь, существует ли инструкция / верно ли она закодирована». Это прямая
дифференцирующая ниша для TrothByte.

---

## 4. Декомпиляция/дизассемблер: «выглядит правильно, но не работает»

Ключевые количественные якоря:
- Meta LLM Compiler: 14% exact match (86% неверных транспозиций) — DEC-1.
- DeGPT: Corrected Fix Rate 37% — ~63% «исправлений» остаются неверными — DEC-3.
- SCDBench: 7% идеальная декомпиляция — DEC-6.
- Capability cliff на ~200 инструкциях + расхождение метрик compile@k vs pass@k —
  DEC-8 (критично для дизайна evals: «компилируется» ≠ «правильно»).
- Универсальный провал на комбинированной обфускации (8 моделей) — DEC-13.

Паттерн «правдоподобно, но неверно» повторяется через все источники (SLaDe,
CoDe-R, Context-Guided, FidelityGPT) и подтверждается практиками (DEC-10, DEC-11,
DEC-12). Класс ошибок «ре-исполняемость» — естественный гейт верификации, который
TrothByte уже использует в `binary-analysis-type-recovery` (покрыто косвенно).

---

## 5. Опасный ассемблерный код

- **Режимные переходы/страницы памяти** (ASM-6): неверный identity-map приводил к
  перезагрузке машины; DOS-конвенции строк перепутаны.
- **Bootloader/OS-dev** (ASM-13): копирование образа в несуществующую память →
  panic монтирования ФС.
- **Ассемблеры-самописцы** (ASM-9..11): три отдельных бага в одном AI-собранном
  ассемблере (директивы, символы, кодирование imul) — цепные ошибки кодогенератора.
- **Криптография** (ASM-5): молчаливый развал AES-NI round-trip.
- **SIMD-переполнение** (ASM-18): отсутствие guard'а на 255-итерациях NEON-счётчиков.

---

## 6. Калибровочные данные (для confidence-gating)

| Метрика | Значение | Источник |
|---|---|---|
| Дизассемблер asm→IR: exact match | **14%** (round-trip 45%) | Meta LLM Compiler, 2407.02524 |
| Декомпиляция: re-executability (best direct) | **45.4%** (6.7B); refined V2 64.9% | LLM4Decompile, 2403.05286 |
| Декомпиляция: DeGPT Corrected Fix Rate | **37%** (FR 83%) | FidelityGPT, 2510.19615 |
| Декомпиляция идеальная (EVM) | **7%** (42/600) | SCDBench, 2605.29059 |
| IR→asm функциональная корректность | **44% x86_64 / 36% aarch64** | NeuComBack, 2511.01183 |
| LLM-супероптимизация: test-passing (Claude-opus-4) | **51.5%**; RL → 95.0% | SuperCoder, 2505.11480 |
| Транспиляция x86→ARMv5 / →RISC-V | **79.25% / 88.68%** | 2411.16341 |
| Capability cliff декомпиляции | ~200 инструкций | 2607.06125 |
| Агентный RE: полные решения | **31.5%** (per-instance 61.4%) | SRE-Bench, 2608.11469 |
| Adversarial binaries: unsafe-действие | **35/40** (vs 0/40 clean) | Binaries Talk Back, 2607.12507 |
| Ошибки на peephole-оптимизации | на уровне отдельных инструкций; CoT помогает | 2412.12163 |
| NEON-SIMD: пропуск overflow-guard | переполнение на ~255 итерациях на lane | Lemire, 2026 |
| Ошибки деобфускации | 5 классов; универсальный провал на combined | Deconstructing Obfuscation, 2505.19887 |

**Вывод для калибровки:** «выглядит корректно» в ассемблерном контексте крайне
ненадёжно — baseline-функциональность LLM-ассемблера ~36–51%, exact-match декомпиляции
~7–37%. Гейты должны требовать **машинной проверки**: ассемблирование→дизассемблирование
и сравнение байтов, re-executability-тесты, контрольные ответы (проверенные компилятором).
Метрика compile@k ≠ pass@k (DEC-8) — прямое обоснование, почему evals должны гонять
исполнение, а не сборку.

---

## 7. Кандидаты в evals (historical / FP / adversarial)

**Historical (реальные инциденты с root cause):**
1. **HerraduraKEx PR#33** (ASM-8): неверный стек-офсет `[esp+4]` + невалидный
   Thumb-2 `cbz` на hi-registers — два класса в одном кейсе.
2. **BBoeOS PR#584** (ASM-11): `imul eax,eax,38` обнуляет immediate — парсинг операндов.
3. **BBoeOS PR#506** (ASM-9): `$` в NASM-директивах 3.x.
4. **System1 PR#1** (ASM-13): bootloader копирует 1.44 MB в 0x00200000 при 1 MB RAM.
5. **cbm-filebrowser PR#1** (ASM-14): 6502 `beq` вместо `bcs` — бесконечный цикл.
6. **r/asm DOS→long mode** (ASM-6): неверный identity-map → reboot.
7. **AES-NI round-trip** (ASM-5): молчаливое несовпадение decrypt(encrypt(x)).

**False-positive (ложная уверенность):**
1. **Codex vs Claude на Cerber5.exe** (DEC-12): два противоположных «high confidence»
   вердикта об упаковке — модель переоценивает собственную уверенность.
2. **Quesma Ghidra-MCP** (DEC-11): уверенно «Centipede» вместо River Raid —
   «confidence и accuracy ортогональны».
3. **box64 #4214** (ASM-16): AI-находки, которые автор не может проверить.
4. **ArchCloudLabs** (DEC-10): уверенный разбор shellcode с выдуманной инструкцией.

**Adversarial:**
1. **Binaries Talk Back** (REB-2): representation-confusion атаки на LLM-RE.
2. **Deconstructing Obfuscation** (DEC-13): комбинированная обфускация как
   «универсальный провал» — калибровочный кейс для уровня шума/надёжности.

**Методологические (проверь проверку):**
1. **Cliff на 200 инструкций** + расхождение compile@k vs pass@k (DEC-8).
2. **MintVID** (ASM-17): «прошедший» код, который никогда не исполнялся (`qemu -d exec`
   показал, что AI-asm не выполнялся) — аналог masked-harness из первого survey.

---

## 8. Сверка с репозиторием TrothByte

**Домен `assembly` (5 скиллов):** asm-x86-64-registers-and-addressing,
asm-calling-conventions, asm-inline-asm-constraints, asm-signed-unsigned-branches,
asm-optimizer-artifacts — покрывает корректное чтение/написание x86-64 (✅ ASM-2, ASM-7;
◐ ASM-1, ASM-4, ASM-10).

**Пробелы (не покрыто):**
1. **Проверка существования/валидности инструкций и их кодировки** (ASM-3, ASM-8,
   ASM-9, ASM-11, ASM-21) — ядро «галлюцинации ассемблера». Нет ни одного скилла.
2. **Синтаксические диалекты ассемблеров** (NASM/MASM/GAS/AT&T): case-sensitivity,
   brackets-address-vs-content, size hints, `default rel`, `$`-директивы (ASM-1,
   ASM-12) — 4 класса из ocrosby + AT&T-инверсия.
3. **Проверка декомпиляции/дизассемблера на достоверность** (DEC-1..13): ре-исполняемость,
   byte-round-trip, предупреждение «compile@k ≠ pass@k», provenance-гейтинг (REB-2).
4. **Не-x86 архитектуры**: ARM Thumb-2 (ASM-8b), 6502 (ASM-14, ASM-19), m68k (ASM-17),
   NEON/AArch64 (ASM-18), RISC-V (ASM-7) — домен assembly x86-центричен (это уже
   отмечено в audit-файле: кандидаты RISC-V/RVV).
5. **Shellcode-анализ** (DEC-10) — пересечение с предложенным в audit-файле
   `fuzzing-harness-evidence-gate` и отложенным exploit-направлением.
6. **Опасные режимные переходы/память** (ASM-6, ASM-13) — частично в bootloader-stages.

**Рекомендации (приоритет):**
1. **N** `asm-verification-hallucination-gate` (HIGH): гейты — `nasm/gas/as`-ассемблирование
   и обратное дизассемблирование с побайтовым сравнением; проверка mnemonic по ISA-справочнику
   (Intel SDM / ARM ARM) как «источник истины»; эмуляция (`qemu -d exec`) вместо веры в
   «прошёл сборку». Источники: Intel SDM, ARM ARM, NASM Manual.
2. **N/E** `asm-syntax-dialects` (NASM/GAS/AT&T/MASM): конвенции операндов, регистры,
   директивы, size hints, `default rel`; источник — NASM Manual, System V ABI. (Идеи из
   ocrosby PR#33, r/asm F1/F2.)
3. **E** `binary-analysis-type-recovery` → `disassembly-fidelity` подраздел: re-executability
   и byte-round-trip гейты, признание «plausible-but-wrong» (DEC-3/DEC-6/DEC-8).
4. **EV**-кейсы по §7 (historical/FP/adversarial) с калибровочными данными §6.
5. **E** `asm-inline-asm-constraints`: кейсы молчаливой порчи (ASM-4, ASM-5) и
   неверного размера (ASM-11).

---

## 9. Не удалось проверить

- **arXiv: LLM + shellcode (0 результатов) и LLM + calling convention (1 нерелевантный)**
  — литературный пробел подтверждён на API; НЕ значит «случаев нет», значит «не
  измерено наукой». Случаи (DEC-10) есть только в блогах/транскриптах.
- **Бенчмарк с частотой ошибок уровня mnemonic/opcode/operand** — не найден; ближайшие
  суррогаты перечислены в §6. Вывод о «галлюцинации mnemonics как классе» опирается на
  отдельные кейсы (ASM-3, DEC-10) и NASM-правила (ASM-12), а не на статистику.
- **Битые ссылки в SnailSploit/Claude-Red** и CVE-утверждения — не перепроверялись
  (это материал audit-файла, в этот survey не включён).
- **Каунты ошибок конкретных моделей в CompilerEval (2511.04132)** — в аннотации только
  качественно («low compilation success rates»); детальные классы ошибок в теле статьи
  не открывались.
- **REx86-цифры** (cross-entropy -64.2%, cosine +20.3%) — из аннотации и превью;
  независимо не воспроизводились.
- **Joshuamckiddy (Codex vs Claude)** — одиночный кейс автора; перепроверка на других
  сэмплах не делалась.
- **Сравнение trothbyte-скиллов с конкретными строками внешних репо** — сверка выполнена
  по уровню доменов/скиллов (skills.yaml), не построчного diff'а.
- Поисковики (DDG/Bing/Google) блокировали ботов — глубина полнотекстового поиска по
  форумам ограничена; не найденное ≠ несуществующее.
