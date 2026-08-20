# Architecture — Low-level skills TrothByte

## 1. Цель

Low-Level Engineering Knowledge System for AI Agents: skills + knowledge + source
provenance + routing + dependencies + tools + verification + evaluations + historical
CVEs + adversarial tests + token optimization + progress tracking.

Оптимизируем под BREADTH + DEPTH + UNIQUENESS + CORRECTNESS + VERIFIABILITY + LOW
INITIAL TOKEN COST. НЕ под количество SKILL.md.

## 2. Layout

```
low-level-agent-skills/
├── README.md            # товарный вход: навигация, quick start, статус
├── AGENTS.md            # правила инженерии + resume protocol
├── LICENSE
├── WORKLOG.md           # журнал сессий
├── registry/            # machine-readable состояние (yaml)
│   ├── skills.yaml      # все skills + статусы
│   ├── sources.yaml     # primary sources
│   ├── claims.yaml      # claim → source → section → skill
│   ├── cross-links.yaml # require/recommend/conflict/extend/verify
│   ├── tools.yaml       # shared tooling
│   └── evals.yaml       # eval-наборы
├── roadmap/             # планирование и покрытие
│   ├── research-ingestion.yaml
│   ├── coverage.yaml
│   ├── unique-skills.yaml
│   ├── priorities.yaml
│   └── progress.yaml    # ГЛАВНЫЙ источник состояния
├── research/            # исходные исследовательские документы
│   ├── Анализ скиллов.md
│   └── Энциклопедия — первичные источники и валидация.md
├── skills/<domain>/     # 35 доменов; в каждом README.md с навигацией по skills
│   └── <skill>/
│       ├── SKILL.md     # компактный operational слой
│       ├── references/  # глубокие знания (загружаются по требованию)
│       ├── examples/    # good/bad — верифицированные compile-and-run фикстуры
│       └── evals/       # eval-кейсы этого skill (synthetic/FP/adversarial/CVE)
├── tools/               # shared tooling (не дублировать в skills)
│   ├── lint/            # skill_lint.py, registry_check.py
│   ├── source/          # source_check.py (provenance audit)
│   ├── tokens/          # token_measure.py
│   └── reports/         # gen_domain_readmes.py
└── docs/                # документация
    ├── SKILLS.md        # каталог всех 185 skills (что делает каждый, stability, путь)
    ├── ACKNOWLEDGMENTS.md  # благодарности авторам использованных репозиториев
    └── architecture.md  # этот документ
```

## 3. Skill architecture

SKILL.md — operational (маленький), а не энциклопедия. Единый стандарт — **9 секций**
(контролируется `tools/lint/skill_lint.py`):

1. WHEN TO USE — триггеры
2. WHEN NOT TO USE — анти-триггеры
3. WHAT THE AGENT OFTEN GETS WRONG — failure modes
4. HOW TO REASON CORRECTLY — правила рассуждения
5. WHAT TO VERIFY
6. HOW TO VERIFY
7. WHERE KNOWLEDGE COMES FROM — provenance
8. RELATED SKILLS — cross-references с типом связи
9. EVALUATION — как скилл оценивается

Примеры НЕ являются секцией SKILL.md: они лежат в `examples/good|bad/` (compile-and-run
фикстуры), а метаданные (type/stability/tier/priority) — в `registry/skills.yaml`.
Подробные материалы (references/examples/scripts) загружаются только при
необходимости (progressive disclosure).

## 4. Knowledge architecture

`knowledge/` — shared references, используемые несколькими skills (ABI-таблицы,
standards-выдержки, version-таблицы). Не дублировать в skills — ссылаться.

`patterns/` — good/bad/gotchas, кросс-skill.

Reference format:
```
RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE
```

## 5. Routing

- Registry-driven: selector видит name/description (не триггеры в теле — issue #216).
- Minimal relevant expansion: активация skill загружает только `require`, `recommend` по запросу.
- Meta-skills (routing/evidence/assumptions/verification/completion) управляют
  разграничением KNOWN / INFERRED / UNVERIFIED.

## 6. Dependency graph

`registry/cross-links.yaml`: require | recommend | conflict | extend | verify.
Не загружать весь dependency tree автоматически.

## 7. Source model

- Primary sources (стандарты/мануалы/ABI/официальная документация) — authority для нормативных утверждений.
- Secondary repositories — только для поиска пробелов и сравнения, НЕ authority.
- Каждое нормативное утверждение traceable: claim → source → section → skill.

## 8. Evaluation model

- synthetic (easy/medium/hard/adversarial, positive/negative/ambiguous)
- historical CVE (DETECT/EXPLAIN/FIX/VERIFY, root cause)
- adversarial (compiles-but-wrong, tests-pass-but-wrong, etc.)
- routing (task → minimal skill set)
- regression (after every change)
- false-positive (precision/recall/FP-rate/warning density)

## 9. Progress model

`roadmap/progress.yaml` — единственный источник состояния. После каждого
существенного действия обновлять progress.yaml + WORKLOG.md.

## 10. Versioning & stability

stability ladder: draft → researched → source-backed → evaluated → stable → deprecated.
Stable требует: source-backed + verification + evals + routing validation + token
validation + license validation.

## 11. Token strategy

- small activation layer (SKILL.md), large optional knowledge layer (references).
- Измеряем metadata/SKILL.md/reference/examples tokens.
- Оптимизируем INITIAL CONTEXT COST, не total repository size.
- Избегаем: огромные SKILL.md, повторение информации в 20 skills, копирование ABI-доков.

## 12. Licensing strategy

Разделяем inspiration / paraphrase / direct quotation / copied structure / copied code /
copied documentation. Храним provenance. Не копируем материалы с несовместимой лицензией.

## 13. Current status (2026-08-20)

> **Auto-regenerated data below is read from `registry/skills.yaml`, `registry/sources.yaml`,
> `registry/claims.yaml`, `registry/cross-links.yaml` — keep those files in sync.**

- **185/185 зарегистрированных skills реализованы** в 35 доменных каталогах; в каждом домене
  есть `README.md` для навигации (генератор: `tools/reports/gen_domain_readmes.py`).
- **93 source-backed** (верифицированы GCC 16.1, rustc 1.97.1, gdb 17.2, as/objdump,
  CMake/Ninja); **92 researched** требуют недоступного в dev-среде тулчейна (NVIDIA CUDA,
  Linux eBPF, LLVM, QEMU, sanitizer-рантаймы, Zig, seL4, Kani/Verus и др.) и честно помечены
  с точными командами верификации в `evals/README.md`.
- Registry: 278 источников, 184 claims с полной provenance, ~322 cross-links
  (require/recommend/conflict/extend/verify) + collision rules.
- Quality gates (v2.0): `tools/lint/skill_lint.py` (9 секций, body ≤250 строк,
  description ≤50 слов), `tools/lint/registry_check.py` (целостность + циклы в require-графе),
  `tools/source/source_check.py` (provenance), `tools/lint/claim_extractor.py`
  (вычитка SOURCE-цитат), `tools/lint/prose_lint.py` (advisory prose checks) — все чистые.
  CI: `.github/workflows/ci.yml` прогоняет `tools/validate.py` + stale-проверки каталога.
- Token-стратегия: SKILL.md ≈ 1–2K токенов; глубина — в `references/` (progressive disclosure),
  бюджет измеряется `tools/tokens/token_measure.py`.
- Актуальные счётчики и следующий шаг: `roadmap/progress.yaml`.
