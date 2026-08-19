# AGENTS.md — Low-level skills TrothByte: Engineering Rules & Resume Protocol

## Resume protocol (каждый запуск)

1. Читать этот файл.
2. Читать `registry/skills.min.yaml` — сжатый индекс (163 скилла: токен-бюджеты, триггеры, requires).
   НЕ читать `docs/SKILLS.md` — 200+ KB, человеческий каталог.
3. Маршрутизировать через `registry/triggers.yaml` (keyword → skill ids) → открыть ТОЛЬКО нужный SKILL.md.
4. Читать `roadmap/progress.yaml` (главный источник состояния). `WORKLOG.md` — по необходимости.
5. НЕ начинать выполненные этапы заново: проверять `status` перед каждой задачей.

## Token budget при подключении

- `docs/SKILLS.md` НЕ читать (200+ KB, для людей).
- Вместо него — `registry/skills.min.yaml` (~10 KB).
- SKILL.md открывать только для выбранных скиллов; `references/` — по требованию (progressive disclosure).

## Engineering rules

1. Skill без записи в registry не создавать.
2. Uniqueness — только после gap analysis.
3. Чужой skill-репозиторий — не единственный authority.
4. Нормативные утверждения — из первичных источников.
5. Сохранять provenance.
6. Копировать текст источников только с разрешения лицензии.
7. SKILL.md — operational и компактный.
8. Глубина — в `references/`.
9. Детерминированные операции — в скрипты (`tools/`).
10. Mature skill требует метод верификации.
11. Важный skill требует eval.
12. Неопределённость помечать: KNOWN / INFERRED / UNVERIFIED.
13. Не помечать работу выполненной молча.
14. Обновлять `roadmap/progress.yaml` после каждой завершённой единицы.
15. Перед коммитом: `python tools/validate.py` (skill_lint + registry_check + source_check).

## Token economy

1. Подавлять шум: `2>$null`, `-q`, усечение вывода.
2. Читать выборочно: `grep`/`glob`/`read` с `limit`/`offset`.
3. Параллелить независимые вызовы инструментов.
4. Валидаторы — один раз, усечённый вывод.
5. Итоговые сообщения — 3–5 буллетов, без пересказа кода.
6. Относительные пути + `workdir`.
7. Между несвязанными задачами — свежая сессия или `/compact`.

## Core development rule

Do not optimize this repository for number of SKILL.md files.

Optimize for: breadth, depth, unique value, primary-source grounding, cross-layer
correctness, executable verification, historical and adversarial evaluation, low
activation-token cost, progressive disclosure, reproducible development state.

A skill is complete only when:
DISCOVERED → DIFFERENTIATED → SOURCE-BACKED → IMPLEMENTED → VERIFIED → EVALUATED →
CALIBRATED → TOKEN-OPTIMIZED → REGISTERED → MARKED COMPLETE.

## Phases (порядок, не пропускать)

0. bootstrap → 1. research ingestion → 2. coverage matrix → 3. unique skills →
4. priorities → 5. source registry → 6. claims/provenance → 7. architecture →
8. skill schema → 9. knowledge layer → 10. behavior layer → 11. tooling →
12. dependency graph → 13. implement skills → 14. token optimization →
15. synthetic evals → 16. false-positive evals → 17. historical CVE evals →
18. adversarial evals → 19. routing evals → 20. collision testing → 21. calibration →
22. license audit → 23. quality gates → 24. progress management → ...

## Stopping rule (контекст > 70%)

1. Сохранить промежуточные результаты.
2. Обновить `roadmap/progress.yaml` (current_task, current_subtask, next_action).
3. Обновить `WORKLOG.md`.
4. Сохранить найденные источники.
5. Вызвать `/compact`.
