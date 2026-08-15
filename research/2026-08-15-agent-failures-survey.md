# Survey: задокументированные отказы ИИ-агентов в low-level/системном программировании

**Дата:** 2026-08-15 · **Статус:** исходная аналитика для новых/доработанных скиллов
**Репозиторий:** TrothByte/low-level-skills-trothbyte

---

## 1. Дата и метод исследования

**Дата проведения:** 2026-08-15.

**Метод.** Поиск распределён между 5 параллельными субагентами, каждый обязан был
проверять каждый источник фактическим fetch'ем (webfetch / API), а не пересказывать:

| Субагент | Направление | Инструменты проверки |
|---|---|---|
| A | Reddit и форумы (r/rust, r/cpp, r/C_Programming, r/embedded, r/kernel, r/ClaudeAI, r/LocalLLaMA, r/ExperiencedDevs) | old.reddit.com (www.reddit.com блокирует), DuckDuckGo HTML |
| B | GitHub issues/PRs (в т.ч. claude-code, cursor, aider, codex; крупные C/C++/Rust/embedded проекты) | `gh search issues/prs`, `gh api search/issues` — живые данные API |
| C | Блоги инженеров и security-фирм, LinkedIn, разбор Ghostty | прямой fetch блогов, Brave Search, HN |
| D | Hacker News + X/Twitter | Algolia HN API (`hn.algolia.com/api/v1`), Wayback для The Register |
| E | Академия (arxiv 2025–2026) | arxiv API (`export.arxiv.org/api/query`) + страницы abs/ |

**Якорные источники (заданы заранее) верифицированы напрямую:**

1. **CONCUR** — arXiv:2603.03683 ✓ (существует, 43 базовые задачи + 72 мутанта = 115,
   чекаются deadlocks/races). Категория «ST / Single Thread» и детали «фейкового
   параллелизма» (модель генерирует thread-safe примитивы, но исполняет всё в одном
   потоке) — **по данным заказчика, в аннотации не названы явно** → помечено UNVERIFIED
   на уровне аннотации.
2. **RustEvo²** — arXiv:2503.16922 ✓ (588 API-изменений: 380 std + 208 из 15 крейтов;
   65.8% стабилизированные vs 38.0% поведенческие; 56.1% до cutoff vs 32.5% после;
   RAG +13.5%).
3. **Крейт-галлюцинации в Rust** — arXiv:2606.08444 ✓ (независимая от модели
   устойчивая частота галлюцинаций, низкая чувствительность к параметрам декодирования;
   принят на Internetware'26).
4. **Ghostty** — **корректировка даты/источника**: инцидент реальный, но первичный
   разбор root cause — это **mitchellh.com, 10 января 2026** (а не Medium, май 2026).
   Пост на Medium от 2 мая 2026 — ретроспектива-комментарий (F7 субагента C, тело не
   fetched). Ключевой факт: Claude Code — **триггер**, а не причина (латентная утечка
   в пуле страниц `PageList.zig` Ghostty, Zig/`mmap`/VM-уровень, `munmap` никогда не
   вызывался; утечка существует с v1.0).
5. **Грег Кроа-Хартман** — верифицировано через The Register от 26 марта 2026
   (Стивен Воган-Николс), точная цитата: «*It spit out 60: "Here's 60 problems I found,
   and here's the fixes for them." About one-third were wrong, but they still pointed out
   a relatively real problem, and two-thirds of the patches were right.*» (60 проблем;
   ~⅓ неверны, но указывают на реальную проблему; ~⅔ патчей корректны).

**Ограничения поиска (важно):**
- **LinkedIn** — login-wall, ни одного поста не удалось заfetchить (субагент C честно
  пропустил; ничего не выдумано).
- **X/Twitter** — login-wall; DDG/Google/Bing блокируют ботов. Ссылки на твиты внутри
  HN-комментариев (FFmpeg/curl) помечены UNVERIFIED.
- **Medium** частично недоступен для fetch (retrospective Ghostty).
- **DuckDuckGo HTML** — CAPTCHA после 2 запросов; Bing возвращал только маркетинг.
  Рабочие каналы: old.reddit, `gh`/GitHub API, HN Algolia, Brave Search, arxiv API, Wayback.

---

## 2. Сводная таблица всех находок

Легенда покрытия: ✅ **покрыто явно** · ◐ **покрыто косвенно** · ✖ **не покрыто**.
Рекомендации: **N** = новый скилл, **E** = дополнение существующего, **EV** = eval-кейс.

### Concurrency
| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| CON-1 | arXiv:2603.03683 (CONCUR, 2026-03) | «Фейковый параллелизм»: thread-safe примитивы при однопоточном исполнении (категория ST) | ✖ — ни один concurrency-скилл не проверяет, что код *реально параллелен* | **N** `concurrency-actual-parallelism` / **EV** ST-кейсы |
| CON-2 | openai/codex#37653 (2026-08-09) | zsh: `jobs` внутри command substitution не видит jobs родителя → лимит параллелизма не срабатывает → 86 конкурентных процессов, watchdog kernel panic, 2 перезагрузки | ✖ | **EV** historical + **E** concurrency/references |
| CON-3 | HN item 47721953 (2026-04-10), комментарий про races/locking/lifetimes | Код «проходит Friday review, дедлокается под нагрузкой через 3 недели» — races/locking/lifetimes как класс уверенных ошибок моделей | ◐ — concurrency-скиллы учат корректности, но не верификации «пройдёт ли review» | **EV** FP + **E** meta-verification |
| CON-4 | arXiv:2602.19594 (ISO-Bench, 2026-02) | Агенты верно находят bottleneck, но ядра не работают (off-by-one, неверные shapes, пропущенные sync-barriers, «проходят review и сегфолтят») | ✖ (см. GPU-секцию) | **EV** historical (GPU) |

### Rust / API-drift
| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| RST-1 | arXiv:2503.16922 (RustEvo²) | API-эволюция: 65.8% стабилизированные vs 38.0% поведенческие; 56.1% до cutoff vs 32.5% после | ✖ — rust-скиллы не покрывают version/API drift | **N** `rust-api-evolution` |
| RST-2 | bevyengine/bevy#23867 (2026-04-17) | Claude Code/Cursor/Copilot ссылаются на удалённые API, несуществующие крейты, устаревшие паттерны Bevy | ✖ (drift + supply chain) | **N** (см. RST-1, SUP-1) |
| RST-3 | r/rust vhz1pk (2022) | Copilot: код не компилируется, игнор ownership, спам `unwrap()`, устаревший `format!` | ◐ — rust-unsafe-reasoning не про version drift; частично про unsafe | **E** rust-unsafe-reasoning + **EV** |
| RST-4 | arXiv:2604.27001 (2026-04) | Крипто-Rust: 23.3% компилируются, 57% из скомпилированных уязвимы, nonce reuse, API-галлюцинации, CoT в 5 раз хуже zero-shot | ✖ — zeroize-constant-time покрывает только constant-time | **N** `rust-crypto-primitives` / **E** zeroize |
| RST-5 | anthropics/claude-code#82057 (2026-07-28) | Отладка регрессии рендеринга: 3 «прошедших» harness'а маскировали дефект (безусловный full repaint), пропущен контекст (файл 34 KB vs искал баг в 64 KiB) | ◐ — meta-verification: валидность harness'а не проверяется | **EV** FP + **E** meta-verification |
| RST-6 | StellarLend/stellarlend-contracts#1454 (2026-07) | Несуществующий символ `require_admin` (компиляторная ошибка), утроенная access-control логика | ◐ (баг верифицирован; **происхождение от ИИ UNVERIFIED**) | **EV** (осторожно) |
| RST-7 | arXiv:2503.02335 (RustBrain, 2025) | Ремонт UB в unsafe-Rust; 94.3% pass / 80.4% execution на Miri | ◐ — rust-unsafe-reasoning | **EV** + **E** references (Miri-гейты) |

### Supply chain / зависимости (домена НЕТ)
| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| SUP-1 | arXiv:2606.08444 (2026-06) | Галлюцинации крейтов: несуществующие крейты, похожие на реальные (опечатка/дефис); частота устойчива между моделями | ✖ — домена нет | **N** `rust-dependency-supply-chain` |
| SUP-2 | arXiv:2406.10279 (USENIX'25) | «We Have a Package for You»: 576k примеров, 5.2% (коммерческие) / 21.7% (open-source) галлюцинаций пакетов | ✖ | **N** (PyPI/npm; перенести метод на crates/C/C++) |
| SUP-3 | arXiv:2505.05057 (MARIN/APIHulBench, 2025) | Метрики API-галлюцинаций (MiHN/MaHR); RAG недостаточен (−67.5% MiHN только с dependency-aware decoding) | ✖ | **N**/в references RST-1 |

### C
| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| C-1 | r/C_Programming 17sa1fi (2023) | Конкретные UB-ловушки: `isspace` на signed `char` (~50% случаев UB), потеря указателя при `realloc` (не проверен NULL), string-literal в `switch`, конкатенация `+` | ◐ — c-undefined-behavior / c-string-and-buffer-safety / c-integer-promotion покрывают классы, не конкретику | **E** references + **EV** |
| C-2 | r/C_Programming 1ufsz4s (2026-06) | `fork`+`shm_open`: ChatGPT даёт неработающий код (segfault/bus error), спасение — man-страница | ✖ — POSIX IPC/shmem не покрыт | **E** c-errno-and-syscall-returns + **EV** |
| C-3 | Aider-AI/aider#3291 (2025-02) | FP-линтер: валидный AUTOSAR C (`#if/#else/#endif`-сплит, `static inline`) помечается как syntax error | ✖ — препроцессор/условная компиляция не покрыты | **EV** FP + **E** c |
| C-4 | arXiv:2108.09293 (Pearce et al., IEEE S&P 2022) | Copilot: ~40% из 1,689 программ уязвимы (89 сценариев, MITRE Top-25 CWE) | ◐ — классы частично в c-скиллах | **E** калибровка (см. §4) |
| C-5 | arXiv:2208.09727 (Lost at C, USENIX'23) | N=58: в узкой задаче (singly-linked list в C) AI-ассист не добавил >10% критических багов | ◐ — контрпример | **E** калибровка (§4) |
| C-6 | arXiv:2604.03610 (DebugHarness, 2026) | Агенты лечат баги как статическую кодогенерацию; use-after-free/memory corruption требуют динамического контекста; ~90% патчей с динамикой | ◐ — meta-verification / c-string-and-buffer-safety | **E** meta-verification |

### C++
| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| CPP-1 | r/cpp 1uv5k2n (2026-07) | 2M-LOC low-latency C++: агент пишет «junior-код» — C-указатели без null-проверки вместо ссылок, всё в свободные функции | ◐ — cpp-object-lifecycle/raii частично | **E** cpp-object-lifecycle + **EV** |
| CPP-2 | Aider-AI/aider#4325 (2025-07) | Транкация C++ файлов при редактировании, даже при корректном diff (связано с auto-add) | — инструментальный баг, не кодоген | пропустить (вне scope) |
| CPP-3 | obsproject/obs-studio#13090 (2026-02) | Claude Code сгенерировал PR с новым публичным C API (`obs_encoder_request_keyframe`, callback, ABI/API-изменение) — отклонён политикой «no AI code» | ◐ — abi-layout-reasoning учит layout, не политику API | **EV** + governance-нота |
| CPP-4 | arXiv:2607.00107 (Illusion of Safety, 2026) | 8,918 C++ программ, 4 уровня верификации: AI-код **~2× чаще** человеческого даёт подтверждённые runtime-нарушения; статический анализ создаёт иллюзию безопасности | ✖ — нет скилла про многоуровневую верификацию | **N**/meta-verification + **EV** FP + калибровка |

### Embedded / firmware
| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| EMB-1 | r/embedded 1tlcr42 (2026-05) | ST7789-драйвер (ATSAMD21G18A): ChatGPT, затем Claude — код не работает (неверные MADCTL-биты, нет частоты SPI, попиксельная запись без framebuffer) | ✖ — нет скилла по периферии/регистрам | **N** `embedded-hw-register-verification` |
| EMB-2 | r/embedded 1nsoeko (2025-09) | Паттерн: «красивый код, который не работает»; галлюцинация имён регистров; нет данных по embedded (кроме ESP32) | ✖ | **N** (EMB-1) |
| EMB-3 | HN 44915206 (2025-08) | Cursor: I2C-драйвер для STM32 — несуществующие регистры, HAL-функции из «чужой» семейства чипов | ✖ | **N** (EMB-1) |
| EMB-4 | reversetobuild.com (2026-03) | Zephyr: галлюцинации Kconfig-символов, devicetree `compatible`-строк, коллизии node-address, бит-сдвиг/endianness в протоколах, register map'ы (reserved bits), ISR «функционально верны, но не оптимизированы» (volatile/cache/barriers) | ✖ | **N** (EMB-1) + **E** embedded-volatile |
| EMB-5 | arXiv:2506.11003 (EmbedAgent, ICSE'25) | DeepSeek-R1 55.6% pass@1 (с принципиальными схемами), ESP-IDF только 29.4%; reasoning-модели «переусложняют» | ✖ (калибровка) | **E** калибровка (§4) |
| EMB-6 | arXiv:2606.16190 (Embedded Arena, 2026) | 0% деплоя на реальном железе без hardware feedback; с feedback — успех за ≤3 итерации | ✖ — скилл про hardware-in-the-loop | **N**/meta-verification |
| EMB-7 | arXiv:2509.09970 (2025) | FreeRTOS-фирма: buffer overflow (CWE-120), **race conditions (CWE-362)**, DoS (CWE-400); итеративная валидация+патчинг дала 92.4% remediation | ◐ — rtos-concurrency-and-isr покрывает паттерны | **EV** (CWE-362 кейс) |

### Kernel
| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| KRN-1 | The Register, 2026-03-26 (Кроа-Хартман) | Эксперимент: 60 проблем, ~⅓ неверных (но указывают на реальную проблему), ~⅔ патчей корректны | ✖ (калибровка FP на ядре) | **E** калибровка (§4) + **EV** FP |
| KRN-2 | Phoronix, 2026-03 (Sashiko, Google/Linux Foundation) | AI-ревьюер LKML: recall ~53% на n=1,000, ~47% багов пропущено, FP <20% | ✖ (калибровка) | **E** калибровка (§4) |
| KRN-3 | The Register, 2026-05-18 (Торвальдс/Tarreau) | AI-репорты захлестнули security-рассылку: 2–3/нед → 10/нед → 5–10/день; дубли от одного LLM; новая политика «AI-найденный баг = публичный» | ✖ (процесс) | **E** калибровка (§4) |
| KRN-4 | dmitrybrant.com, 2025-09-07 | Модернизация ftape-драйвера (2.4→6.8): компилируется, но драйвер не видит железо (ENXIO, base-address = -1 → 0xffff); нужны ручные фиксы и знание kernel internals | ◐ — kernel-скиллы про атомарность/RCU/uaccess, не про device/IO-логику | **EV** historical |
| KRN-5 | openai/codex#32181 (2026-07) | Safety-guardrails постоянно срабатывают на C-коде ядер/драйверов/HPC — инструмент становится непригодным | — инструментальный, вне scope скиллов | пропустить |

### GPU
| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| GPU-1 | arXiv:2602.19594 (ISO-Bench) | Ядра «проходят review, но сегфолтят под нагрузкой»: off-by-one, неверные shapes, пропущенные barriers | ✖ — ptx-assembly/gpu-memory-model не покрывают генерацию ядер | **EV** historical |
| GPU-2 | arXiv:2606.20128 (Correctness Illusion, 2026) | Оракулы KernelBench/TritonBench/GEAK (allclose на фиксированном shape) сертифицируют бажные ядра как корректные; fuzz+fp64-reference ловит 9/9, controls 15/15 | ✖ — методология верификации GPU | **N**/**E** verification + **EV** |
| GPU-3 | arXiv:2608.04450 (CommBench, 2026-08) | GPU-коммуникации: сильнейшая модель (GPT-5.5) верно + с competitive-перфомансом только 30.7% из 100+ задач | ✖ (калибровка) | **E** калибровка (§4) |
| GPU-4 | arXiv:2605.16819 (AgentKernelArena, 2026) | Обобщение на невиданные конфигурации: near-perfect на seen, резкое падение корректности на unseen shapes | ✖ | **EV** generalization |
| GPU-5 | arXiv:2605.23215 (FastKernels, 2026) | Sandbox-метрики ≠ production: 0.94× совокупный speedup у лучшего агента | ◐ — performance-measurement-discipline про валидность замеров | **E** performance-measurement-discipline |
| GPU-6 | obsproject/obs-studio#13227 (2026-03) | AI-сгенерированный Vulkan shader pipeline PR отклонён политикой | — governance | пропустить |

### Compiler / LLVM
| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| COM-1 | arXiv:2607.00700 (LLVM-Bench, 2026) | Доминируют invalid patches и build failures; лучший ансамбль решает 21.99% из 423 issue | ◐ — llvm-pass-writing/llvm-ir-reading частично | **EV** historical |
| COM-2 | arXiv:2607.02684 (PeepholeBench, 2026) | Неверное использование LLVM-механизмов; ни один агент не совпал с человеком по корректности+profitability | ◐ — llvm-pass-writing | **E** llvm-pass-writing + **EV** |
| COM-3 | arXiv:2603.20075 (llvm-harness, 2026) | Ремонт багов LLVM middle-end: harness улучшает frontier-модели на 62% | — методика, не фейл | пропустить |
| COM-4 | arXiv:2511.04132 (CompilerEval, 2025) | LLM как end-to-end компилятор: низкий процент успешной компиляции | ◐ — assembly/compiler | **E** references |
| COM-5 | arXiv:2508.03603 (ReFuzzer, 2025) | LLM-генерация тестов для компилятора: 47–49% валидных → 96.6–97.3% после фидбек-ремонта | — методика | пропустить |
| COM-6 | arXiv:2509.16671 (Digital Camouflage, 2025) | LLVM-обфускация снижает precision/recall/F1 LLM-детекции уязвимостей в C | — adversarial | **EV** adversarial (опц.) |

### Reverse engineering / assembly / FFI
| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| RE-1 | arXiv:2606.16162 (AutoDecompiler, 2026) | Декомпиляция «компилируется, но отклоняется от поведения бинарника» | ◐ — binary-analysis-type-recovery | **EV** + **E** type-recovery |
| RE-2 | r/cpp 1ulzpuf (2026-07) | Claude-агенты галлюцинируют call paths/sequence diagrams легаси C/C++ | ◐ — binary-analysis-type-recovery/go-rust-re | **E** |
| RE-3 | HN 36498370 (2023) | Windows FFI: `GetShellWindow()` + null PIDL в `SHGetPathFromIDListW` + проверка titlebar «File Explorer»; ObjC-вариант не компилируется | ◐ — ffi-boundary-cross-language | **EV** |

### VM / memory allocator
| ID | Источник | Механизм | Покрытие | Рекомендация |
|---|---|---|---|---|
| VM-1 | mitchellh.com/writing/ghostty-memory-leak-fix, 2026-01-10 (+ HN 46568794, 632 pts; issues #10289, #10258) | Латентная утечка в пуле страниц Ghostty (Zig, `mmap`): нестандартные страницы переиспользовались, `munmap` не вызывался (VM-утечка); Claude Code — триггер при масштабе; 37–130 GB в отчётах | ✖ — нет скилла про аллокаторы/VM-семантику | **EV** historical + **EV** FP (нарратив «Claude Code сломал Ghostty» — ложная атрибуция) |

---

## 3. Подробный разбор приоритетных пробелов («не покрыто»)

Этот раздел — приоритет для дальнейшей работы. Каждая находка: источник, механизм,
частота/подтверждённость, сверка с репозиторием, рекомендация.

### 3.1 «Фейковый параллелизм» — код выглядит thread-safe, но не параллелен (CON-1)

- **Источник:** arXiv:2603.03683 (CONCUR, 2026-03-04), субагент E.
- **Механизм:** Модели используют `ConcurrentHashMap`/`atomic`/`synchronized`, но
  выполняют всё в одном потоке. Бенчмарк (43 задачи из учебника по concurrency +
  72 мутанта) ловит deadlocks и races, которые линейные бенчмарки типа HumanEval не
  видят. Детали категории ST — по данным заказчика (см. §6).
- **Частота:** количественный бенчмарк; вывод — «existing benchmarks… cannot be useful
  for evaluating concurrent code generation» (аннотация).
- **Сверка с репозиторием:** `concurrency/` содержит 4 скилла (memory-ordering-reasoning,
  atomics-c11-cpp11-rust, deadlock, condvar). **Все они проверяют корректность уже
  параллельного кода; ни один не проверяет, исполняется ли код на самом деле на
  нескольких потоках.** Пробел подтверждён.
- **Рекомендация:** новый скилл `concurrency-actual-parallelism` (гейты: реально N
  потоков в планировщике, `ps -eLf`, `sched_getaffinity`, отсутствие «sequence-equal»
  исполнения) + eval-кейсы по ST-категории. Альтернатива минимального объёма —
  добавить проверку параллелизма в `meta-verification`.

### 3.2 Rust API/version drift — deprecated std/tokio, поведенческие изменения (RST-1)

- **Источник:** arXiv:2503.16922 (RustEvo²), субагенты A/E (верифицировано напрямую).
- **Механизм:** 588 синтезированных API-изменений (380 std, 208 из 15 крейтов), 4
  категории: Stabilizations, Signature Changes, **Behavioral Changes**, Deprecations.
  Модели: 65.8% на стабилизированных vs **38.0% на поведенческих** (та же сигнатура,
  другая семантика); 56.1% до cutoff обучения vs **32.5% после**; RAG +13.5%.
- **Частота:** количественный бенчмарк, 2026.
- **Сверка с репозиторием:** `rust/` = rust-unsafe-reasoning, rust-ffi-boundary,
  rust-panic-safety. **Version drift не покрыт.** Паттерн подтверждается и из полей:
  bevy#23867 (удалённые API/несуществующие крейты), r/rust (устаревший `format!`).
- **Рекомендация:** новый скилл `rust-api-evolution` — чек-лист: проверка сигнатуры
  против установленной версии (`cargo doc`, `cargo check`, `rustc --edition`), 
  признание «behavioural change» как самого коварного класса, гейты на `#[deprecated]`,
  семантические diff по версиям. + eval-кейсы по 4 категориям RustEvo².

### 3.3 Crate/dependency/supply-chain — домена нет (SUP-1..3)

- **Источник:** arXiv:2606.08444 (крейт-галлюцинации, 2026-06), arXiv:2406.10279
  (USENIX'25: 5.2%/21.7% галлюцинаций пакетов), arXiv:2505.05057 (MARIN).
- **Механизм:** Модели придумывают несуществующие крейты, похожие на реальные через
  опечатку/пропущенный дефис (типосквоттинг-риск). В Rust галлюцинации стабильны
  между моделями и малочувствительны к декодированию — это отличается от Python/JS.
- **Частота:** крупное эмпирическое исследование + 576k-выборка.
- **Сверка с репозиторием:** в `registry/skills.yaml` нет ни одного домена
  supply-chain/dependencies. **Полный пробел.**
- **Рекомендация:** новый скилл `rust-dependency-supply-chain`: гейты на `cargo add`
  только после проверки существования крейта (`cargo search`, crates.io API),
  сравнение ближайших имён (typosquatting), `cargo deny`/`cargo audit`, минимальные
  версии. Перенести методологию из «We Have a Package for You» на crates.

### 3.4 Embedded: галлюцинация регистров/периферии — «красивый код, который не работает» (EMB-1..4)

- **Источник:** r/embedded 1tlcr42 (ST7789, 2026), r/embedded 1nsoeko (2025),
  HN 44915206 (Cursor STM32 I2C, 2025), reversetobuild.com (Zephyr, 2026).
- **Механизм:** Модели галлюцинируют имена регистров, биты reset-значений, reserved
  bits, `compatible`-строки devicetree, Kconfig-символы; путают семейства чипов
  (STM32 vs ESP32 HAL); не задают частоту SPI/тактовый делитель; пишут попиксельно
  без framebuffer/DMA. Итог — «beautifully looking code that doesn't work».
- **Частота:** паттерн из 4+ независимых источников (сильный сигнал).
- **Сверка с репозиторием:** embedded-скиллы (mpu-trustzone, volatile, rtos-isr,
  interrupt-nested, linker-script) покрывают безопасность и concurrency, но **не
  работу с hardware-регистрами и datasheet-ориентированную периферию**.
- **Рекомендация:** новый скилл `embedded-hw-register-verification` (гейты: сверить
  имя регистра/бита с datasheet, проверить reset-значения, clock enable, GPIO mux,
  наличие framebuffer/DMA); + eval-кейсы (ST7789 MADCTL, STM32 I2C). Дополнить
  `embedded-volatile-and-memory-ordering` ISR-оптимизациями (EMB-4).

### 3.5 GPU: «иллюзия корректности» — ядра проходят проверку, но неверны (GPU-1..4)

- **Источник:** arXiv:2602.19594 (ISO-Bench), arXiv:2606.20128 (Correctness Illusion),
  arXiv:2608.04450 (CommBench), arXiv:2605.16819 (AgentKernelArena).
- **Механизм:** (1) оракулы вида allclose на одном фиксированном shape сертифицируют
  бажные LLM-ядра как корректные — fuzz с fp64-референсом ловит 9/9, controls 15/15
  (идентичные вердикты на 5 классах GPU); (2) агенты верно находят bottleneck, но
  генерируют ядра с off-by-one/неверными shapes/пропущенными barriers, которые
  «проходят review, а сегфолтят под нагрузкой»; (3) провал на невиданных конфигурациях.
- **Частота:** 4 независимых количественных исследования 2026 г. (сильный сигнал).
- **Сверка с репозиторием:** `gpu/` = ptx-assembly, gpu-memory-model-coherence —
  оба о проверке корректности модели памяти, **не о генерации ядер и не о слабости
  верификационных оракулов**.
- **Рекомендация:** **EV** historical: «ядра, которые проходят fixed-shape oracle,
  но ломаются на других shape/при нагрузке»; **E** `meta-verification`: обязательность
  fuzzing-подобных гейтов вместо одного фиксированного теста; **E** `ptx-assembly`
  references (sync-barriers, index math).

### 3.6 C++: «иллюзия безопасности» — статический анализ не видит разницы (CPP-4)

- **Источник:** arXiv:2607.00107 (2026-07), 8,918 C++ программ, 4 уровня верификации.
- **Механизм:** AI-код ~2× чаще человеческого вызывает подтверждённые runtime-
  нарушения; при этом cppcheck/clang-tidy показывают одинаковую «чистоту» для AI и
  человека (артефакт длины кода) — статический анализ создаёт ложную уверенность.
- **Частота:** количественное (8,918 программ).
- **Сверка с репозиторием:** частично пересекается с `sanitizer-agent-ci-loop`
  (динамические гейты) и `meta-verification`. Но скилла про **многоуровневую
  верификацию и осознанную слабость статики** нет.
- **Рекомендация:** **E** `meta-verification`/`sanitizer-agent-ci-loop`: добавление
  уровня runtime (ASan/UBSan/BMC) как обязательного для AI-кода; **EV** FP: «код
  чист по статике, но падает в runtime».

### 3.7 Крипто-Rust: не компилируется + уязвимо (RST-4)

- **Источник:** arXiv:2604.27001 (2026-04, EASE'26).
- **Механизм:** 240 сэмплов AES-256-GCM/ChaCha20-Poly1305: 23.3% компилируются,
  из них 57% уязвимы (rule-based анализатор, 0 FP), nonce reuse, API-галлюцинации;
  CoT в 5 раз хуже zero-shot (P=0.002); ChaCha 12.5% vs AES 34.2%.
- **Частота:** количественное.
- **Сверка с репозиторием:** `zeroize-constant-time` покрывает только constant-time
  код. Генерация криптопримитивов не покрыта.
- **Рекомендация:** **N** `rust-crypto-primitives` (или расширение zeroize): гейты —
  компиляция под целевую версию, `cargo test` с известными answer vectors,
  запрет nonce reuse, вызов проверенных крейтов (ring/rustcrypto) вместо
  «изобретения» крипты; + eval-кейсы.

### 3.8 VM/аллокаторы: Ghostty как historical + FP-кейс (VM-1)

- **Источник:** mitchellh.com/writing/ghostty-memory-leak-fix, 2026-01-10; HN 46568794;
  ghostty#10289 (71.49 GB на 16 GB системе), #10258 (gigabytes in seconds при resize).
- **Механизм:** В `PageList.zig` пул страниц фиксированного размера; строки с
  эмодзи/гиперссылками требуют «нестандартные» страницы через прямой `mmap`. При
  оптимизации переиспользования старой страницы сбрасывалась её метаданные-размер,
  но `mmap` оставался большим → на free память считалась пуловой и **`munmap` не
  вызывался** — VM-утечка, не heap. С v1.0; триггер — массовое использование
  Claude Code в терминале (постоянные repaint/эмодзи).
- **Частота:** паттерн (много юзеров), количественные отчёты 37/71.49/96/130 GB.
- **Сверка с репозиторием:** скилла про аллокаторы/виртуальную память нет.
- **Рекомендация:** **EV** historical: «VM-утечка из-за пропущенного munmap»;
  **EV** FP: популярный нарратив «Claude Code вызвал утечку в Ghostty» — ложная
  атрибуция, root cause — латентный баг пула. Отличный кейс для FP-категории evals.

---

## 4. Количественные данные для калибровки false-positive / confidence-gating

Эти цифры — сырьё для настройки гейтов уверенности агентов и FP-порогов evals.

| Метрика | Значение | Источник | Домен |
|---|---|---|---|
| Патчи ядра от AI-инструмента: корректные | **~2/3 (~40 из 60)** | Кроа-Хартман, The Register 2026-03-26 | kernel |
| Патчи ядра: неверные (но указывают на реальную проблему) | **~1/3 (~20 из 60)** | там же | kernel |
| Recall AI-ревьюера LKML | **~53%** (n=1,000), ~47% багов пропущено | Sashiko (Phoronix, 2026-03) | kernel |
| FP-rate Sashiko | **<20%**, большинство «серая зона» | там же | kernel |
| Поток AI-репортов в security-рассылке ядра | 2–3/нед → 10/нед → **5–10/день** | Tarreau, LWN 1065620 (2026-05) | kernel |
| RustEvo²: успех на стабилизированных API | **65.8%** | arXiv:2503.16922 | rust |
| RustEvo²: успех на поведенческих изменениях | **38.0%** | там же | rust |
| RustEvo²: до cutoff обучения vs после | **56.1% → 32.5%** | там же | rust |
| RustEvo²: выигрыш RAG | **+13.5%** (для API после cutoff) | там же | rust |
| Copilot: уязвимые программы | **~40%** (1,689 программ, 89 сценариев CWE) | arXiv:2108.09293 (S&P'22) | C/C++ |
| Lost at C: критические баги при AI-ассисте | **≤10% сверх контроля** (N=58) | arXiv:2208.09727 (USENIX'23) | C |
| Крипто-Rust: компилируемость | **23.3%** (AES 34.2% / ChaCha 12.5%) | arXiv:2604.27001 (2026) | Rust |
| Крипто-Rust: уязвимы из скомпилированных | **57%** (0 FP rule-based) | там же | Rust |
| CoT vs zero-shot (крипто-Rust) | **CoT в 5× хуже** (P=0.002) | там же | Rust |
| C++: runtime-нарушения AI vs человек | **~2× чаще** (8,918 программ) | arXiv:2607.00107 (2026) | C++ |
| GPU-коммуникации: сильнейшая модель | **30.7%** (из 100+ задач) | arXiv:2608.04450 (2026) | GPU |
| GPU-ядра: fuzz-оракул ловит бажные | **9/9**; controls **15/15** чисты; 5 GPU-классов | arXiv:2606.20128 (2026) | GPU |
| LLVM-issue resolution (лучший ансамбль) | **21.99%** (423 issue) | arXiv:2607.00700 (2026) | compiler |
| Галлюцинации пакетов (PyPI/npm) | **5.2%** commercial / **21.7%** open-source | arXiv:2406.10279 (USENIX'25) | supply-chain |
| AI-ревью (winit): «значительная часть FP, но ненулевая валидная доля» | качественно, 1 подтверждённый мейнтейнером | winit#4569 (2026-05) | rust |

**Вывод для калибровки:** у достоверно зафиксированных AI-фейлов в low-level коде
FP-доля находитcя в диапазоне ~20–33% (Sashiko <20%, Kroah-Hartman ~33%), при этом
валидная доля ненулевая и часто «указывает на реальную проблему». Гейты уверенности
должны требовать выполнения динамической/executable проверки, а не принимать
статический анализ или «чистый diff» как подтверждение (см. CPP-4, GPU-2).

---

## 5. Кандидаты в evals/historical и false-positive

### Historical (реальные инциденты с известным root cause)

1. **Ghostty VM-утечка (VM-1):** инцидент с 71.49 GB/16 GB, root cause — пропущенный
   `munmap` для переиспользуемых страниц пула. → исторический кейс «VM/allocator leak».
2. **codex#37653 zsh-параллелизм (CON-2):** `jobs` в command substitution не видит
   jobs родителя → лимит конкурентности не работает → 86 процессов, kernel panic,
   перезагрузка. → historical + concurrency.
3. **ftape kernel-драйвер (KRN-4):** код компилируется, но железо не видно (ENXIO,
   base-address -1→0xffff); «компилируемый ≠ работающий» на драйверном уровне.
4. **ISO-Bench «прошло review, сегфолтит под нагрузкой» (GPU-1):** ядра с
   off-by-one/неверными shapes/пропущенными barriers.
5. **LLVM-Bench invalid patches (COM-1):** патчи не применяются / не собираются —
   failure mode «patch invalidity» как класс.

### False-positive (важно для FP-категории evals)

1. **«Claude Code вызвал утечку в Ghostty»** — ложная атрибуция; реальная причина —
   латентный баг пула (mitchellh: «No AI was used in my work here»).
2. **aider#3291** — валидный AUTOSAR C (`#if/#else/#endif`, `static inline`) помечен
   как syntax error — инструментальный FP.
3. **winit#4569** — AI-ревью: значительная часть находок FP, но есть валидные
   (Android ScaleFactorChanged, подтверждено мейнтейнером).
4. **Kroah-Hartman ~⅓ неверных**, но «указывают на реальную проблему» — грань между
   FP и «недо-точным» finding.
5. **Sashiko FP <20%** — «модель не понимает контекст/код».

### Adversarial / методологические

1. **Correctness Illusion (GPU-2):** оракул fixed-shape allclose сертифицирует баг —
   обязательный кейс «проверь проверку».
2. **claude-code#82057 (RST-5):** «прошедший» harness, который не тестирует целевой
   код (безусловный repaint маскирует баг) — кейс валидности harness'а для
   `meta-verification`.
3. **Digital Camouflage (COM-6):** обфускация против LLM-детекции — опциональный
   adversarial.

---

## 6. Не удалось проверить (честный отчёт)

- **CONCUR, категория ST / «фейковый параллелизм».** Существование бенчмарка и его
  рамки (43+72, deadlock/race) верифицированы по аннотации. Конкретное название
  категории и детали механизма «выполняется в одном потоке» взяты из задания заказчика;
  в аннотации явно не названы → **UNVERIFIED на уровне аннотации** (нужен полный текст).
- **Ghostty, «Medium, май 2026».** Первичный разбор — mitchellh.com, 10 янв 2026
  (верифицирован). Пост Medium от 2 мая 2026 — ретроспектива; **тело не удалось
  fetch'нуть** (транспортные ошибки) — подтверждены только заголовок/дата по поиску.
- **LinkedIn.** Ни один пост не доступен (login-wall); направление постов практиков
  systems-programming из LinkedIn осталось непокрытым. Не выдумано ни одного поста.
- **X/Twitter.** Login-wall; поисковики блокируют ботов. Упоминания твитов FFmpeg/curl
  внутри HN-комментариев не верифицированы напрямую → **UNVERIFIED**.
- **ISO-Bench, детальные пер-модель цифры** (46.2%/26.7%, «2412× I need to actually
  use the tools», «84 mock-файла») взяты субагентом C с проектного сайта, не из
  аннотации → в сводку вынесены осторожно; надёжно подтверждена только аннотация
  («identify correct bottlenecks but fail to execute working solutions»).
- **StellarLend#1454** — технический дефект верифицирован (нет символа `require_admin`);
  происхождение от ИИ-агента **не подтверждено**.
- **mtlynch.io «Claude Code нашёл 23-летнюю уязвимость ядра»** — найден как
  counterpoint; содержание статьи не fetch'илось (только заголовок/метаданные HN).
- **Reddit через www.** Блокируется; работал только old.reddit.com. Возможен
  систематический недохват части тредов.
- **DuckDuckGo/Bing/Google** — бот-защита; систематический полнотекстовый поиск по
  блогам ограничен (заменялся Brave/HN Algolia/arxiv API/gh).

---

## 7. Что это даёт репозиторию (сводно)

**Три подтверждённых пробела заказчика закрываются свежими источниками:**
1. `concurrency/` → CONCUR (CON-1) + codex#37653 (CON-2).
2. `rust/` → RustEvo² (RST-1) + bevy#23867 (RST-2) + r/rust stale-API (RST-3).
3. supply-chain → крейт-галлюцинации (SUP-1) + «We Have a Package» (SUP-2).

**Новые кандидаты в скиллы (по убыванию приоритета):**
`embedded-hw-register-verification` (EMB-1..4) → `rust-api-evolution` (RST-1) →
`rust-dependency-supply-chain` (SUP-1) → `concurrency-actual-parallelism` (CON-1) →
`rust-crypto-primitives` (RST-4) → расширение `meta-verification` (GPU-2, CPP-4, RST-5).

**Кандидаты в evals:** исторические — VM-1 (Ghostty), CON-2 (codex), KRN-4 (ftape),
GPU-1 (ISO-Bench); FP — VM-1 (ложная атрибуция), C-3 (aider#3291), RST-5 (masked
harness), winit#4569; методологические — GPU-2 (Correctness Illusion).
