# AGENTS.md — Low-level skills TrothByte: Engineering Rules & Resume Protocol

## Resume protocol (выполнять при каждом запуске)

1. Прочитать этот файл.
2. Прочитать `roadmap/progress.yaml` (главный источник состояния).
3. Прочитать `WORKLOG.md`.
4. Только после этого продолжать работу.
5. НЕ начинать уже выполненные этапы заново. Проверять `status` перед каждой задачей.

## Engineering rules

1. Never create a skill without a registry entry.
2. Never claim uniqueness without a gap analysis.
3. Never use another skill repository as the sole technical authority.
4. Prefer primary sources for normative claims.
5. Preserve source provenance.
6. Never copy source text unless licensing explicitly permits it.
7. Keep SKILL.md operational and compact.
8. Put detailed knowledge into references.
9. Put deterministic operations into scripts (delegate to shared tools/).
10. Every mature skill requires at least one verification method.
11. Every important skill requires an eval.
12. Mark uncertainty explicitly (KNOWN / INFERRED / UNVERIFIED).
13. Never silently mark work complete.
14. Update progress files after every completed unit.
15. Run repository validators before committing.

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

## Stopping rule (если контекст заканчивается)

1. Сохранить промежуточные результаты.
2. Обновить progress.yaml (current_task, current_subtask, next_action).
3. Обновить WORKLOG.md.
4. Сохранить найденные источники.
5. Не оставлять незаписанную информацию только в памяти модели.
