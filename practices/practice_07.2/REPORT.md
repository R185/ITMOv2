# Практика 7.2 — поддержка пользователя с помощью AI-инструментов

n8n 2.48.6, `https://rreflector.com`, тестовый репозиторий
[R185/itmo-ai-practice-07](https://github.com/R185/itmo-ai-practice-07).

## Состав

| Файл | Что это |
|---|---|
| `workflows/7.2-issue-classifier.json` | Sub-workflow: Execute Workflow Trigger (`title`, `body`) → Basic LLM Chain + Structured Output Parser → Code-нормализация → `type`, `confidence` |
| `workflows/7.2-issue-triage.json` | Workflow 1: вебхук `issues.opened` → классификатор → Switch по типам → лейблы через GitHub API + ветка `author_association` |
| `workflows/7.2-weekly-stats.json` | Статистика: cron `0 9 * * MON` → 4 запроса к GitHub API → Code-группировка → Data Table → Δ → AI-комментарий → HTML-письмо через Gmail |

## Часть 1. Классификация Issues

Классификатор вынесен отдельным сценарием и вызывается нодой **Execute Workflow**.
Основной сценарий: вебхук `issues` → фильтр `action == "opened"` → Sub-workflow →
`Decide Label` (Code) → **Switch by Type** на 4 выхода (`bug`, `task`, `feature`, fallback
`needs-triage`) → HTTP `POST /repos/{owner}/{repo}/issues/{n}/labels` → комментарий с типом
и уверенностью. Для `needs-triage` комментарий не публикуется — только лейбл.

Параллельно от `Decide Label` идёт ветка **First-time Contributor?**: если
`author_association == FIRST_TIME_CONTRIBUTOR`, ставится лейбл `first-time-contributor`
и публикуется приветствие.

| # | Критерий | Баллы | Подтверждение |
|---|---|---|---|
| 1 | Issue классифицируется как `bug`/`task`/`feature`, лейбл проставлен | 2 | Issues #4 `bug`, #5 `feature`, #6 `task` — [issues](https://github.com/R185/itmo-ai-practice-07/issues?q=is%3Aissue). Прогоны: exec 33 (#4, 0.97), 35 (#5, 0.97), 39 (#6, 0.97). Скриншот списка: `evidence/github-issues-list.png` |
| 2 | Issue с `confidence < 0.8` получает `needs-triage` | 1 | Issue #7 «Что-то не работает» → тип `bug`, уверенность **0.6** → лейбл `needs-triage`, комментария нет. Прогон: exec 42. Скриншоты: `evidence/github-issue-7-needs-triage.png`, `evidence/7.2-needs-triage-exec42.png` |
| 3 | Классификатор в Sub-workflow, вызывается через Execute Workflow, поведение не изменилось | 2 | Отдельный сценарий `7.2 Issue Classifier (sub-workflow)` (id `pY8Dd1abFp1CLUe1`), в основном — нода `Classify (Sub-workflow)` с типом `executeWorkflow`. Прогоны классификатора видны отдельными executions: `evidence/7.2-classifier-executions-list.png`; на канвасе видно `Structured Output Parser` и модель. Все 4 issue классифицированы через него (exec 33/35/39/42) |
| 4 | Cron-триггер настроен, 4 запроса к GitHub API за неделю | 2 | Нода `Every Monday 09:00` с выражением `0 9 * * MON` + `Run Manually`. Четыре запроса: `search/issues created:>=`, `search/issues closed:>=`, `repos/{owner}/{repo}`, `search/issues is:pr merged:>=`. Прогон показывает все четыре ноды с данными: exec 69; канвас с четырьмя запросами — `evidence/7.2-stats-executions.png` |
| 5 | Code node группирует статистику по типу и по автору | 2 | Нода `Group Stats` (Code): `by_type {bug, task, feature, needs-triage, other}`, `by_author {login: count}`, плюс `total_created`, `total_closed`, `total_merged_prs`, `open_issues`. В прогоне за неделю 2026-11-16: `by_type {bug:1, task:2, feature:1, needs-triage:1, other:0}`, `by_author {R185:5}`, `total_created 5` |
| 6 | HTML-письмо со статистикой пришло через Gmail node | 2 | Письмо отправлено нодой `Gmail: weekly report` (credential `Gmail OAuth2 (R185)`) на `mdr185x@gmail.com`, прогон exec 69: у сообщения `id 1a1f72db549ab408`, метки `UNREAD, SENT, INBOX` — то есть оно принято Gmail и лежит во входящих. Содержимое, вытянутое обратно через Gmail API: `evidence/7.2-email-weekly-report.png` |
| 7 | Статистика недели сохраняется в Data Table | 1 | Таблица `weekly_stats` (id `0YecZLbSV3HkcyaV`), две строки: 2026-11-09 (`total 6`) и 2026-11-16 (`total 5`). Пишет нода `Save Week Stats` (exec 69). Скриншот: `evidence/7.2-data-table-weekly_stats.png` |
| 8 | В письме показан Δ к прошлой неделе из Data Table | 2 | Нода `Compute Delta` читает прошлую неделю из `weekly_stats` (`week_start < текущей`, DESC, limit 1). В exec 69: `delta_total = -1`, `delta_by_type {bug:-1, task:0, feature:0, needs-triage:0}`. В письме это строка «-1 к прошлой неделе» и колонка «Δ» в таблице по типам, внизу подпись «Δ рассчитан по Data Table (weekly_stats); прошлая неделя: 2026-11-09». Скриншот: `evidence/7.2-email-weekly-report.png` |
| 9 | Блок «AI-комментарий недели» в письме | 2 | Нода `AI Weekly Comment` (Basic LLM Chain) получает статистику и Δ и подставляется в HTML блоком «🤖 AI-комментарий недели». В отправленном письме: «За неделю создано 5 issue и ни одного не закрыто, смерженных PR нет — активность снизилась на 1 к прошлой неделе…». Скриншот: `evidence/7.2-email-weekly-report.png` |

## Скриншоты (`evidence/`)

| Файл | Что видно |
|---|---|
| `7.2-triage-executions-list.png` | список прогонов Workflow 1 |
| `7.2-needs-triage-exec42.png` | прогон размытого issue: ветка `needs-triage` |
| `7.2-classifier-executions-list.png` | прогоны Sub-workflow классификатора |
| `github-issues-list.png` | лейблы по типам в репозитории |
| `github-issue-7-needs-triage.png` | issue #7: `needs-triage`, комментария нет |
| `github-issue-4-bug-labeled.png` | issue #4: `bug` + комментарий классификатора |
| `7.2-data-table-weekly_stats.png` | две недели в Data Table |
| `7.2-stats-executions.png` | канвас последнего прогона статистики, все четыре запроса к API |
| `7.2-email-weekly-report.png` | содержимое письма из Gmail: таблицы, Δ, AI-комментарий |


## Сверхрезультат ★★: от bug report до master (10 баллов)

Сценарий `workflows/7.2-bug-to-master.json` (id в n8n `WyTiO3HFxigvl41t`, 38 нод) — полностью
автоматический путь: issue с лейблом `bug` → патч → ветка → PR → AI-ревью → мерж в `main`, если
серьёзных замечаний нет.

Как устроено:

1. **Вход.** Вебхук GitHub `issues`, фильтр `action in (opened, labeled)` и лейбл `bug`.
2. **Контекст.** Через GitHub API читаются `src/calc.py`, `tests/test_calc.py` и SHA `main`.
3. **Патч.** `deepseek-flash` получает issue, текущий файл и тесты и возвращает полный текст файла.
   Code-нода проверяет, что это действительно код и что он отличается от исходного; при провале —
   один автоматический повтор с другим промптом, потом честный комментарий «нужен человек».
4. **Ветка и PR.** Ветка `fix/issue-N` пересоздаётся с текущего `main` (старая удаляется, прогон
   идемпотентен), коммит через Contents API, PR с `Fixes #N`.
5. **Ревью.** PR попадает в уже готовый сценарий 7.1: точечные комментарии + summary, метки
   🔴 major / 🟡 minor / ⚪ nit.
6. **Гейт.** Ждёт, пока CI и ревью опубликуются (до 6 итераций по 90 с). Мерж только если
   `pytest` зелёный **и** нет 🔴 major. Если major есть — арбитр (`deepseek-flash`) перечитывает
   замечания и дифф и решает, блокирующие они или ложные: ревьюер ошибается регулярно,
   счётчик замечаний без арбитра останавливает корректные патчи.
7. **Финал.** Squash-мерж, комментарий в issue, закрытие. Любой отказ — PR остаётся открытым
   с объяснением причины.

Проверено на трёх живых багах:

| Issue | Что было сломано | Итог |
|---|---|---|
| #10 | `Accumulator()` — общий список на все экземпляры | PR #11 смержен, CI зелёный |
| #12 | `percent_change(old=0)` — деление на ноль | PR #15 смержен, issue закрыт |
| #16 | `summary([])` падал на пустом списке | PR #23 и #24 смержены; прогон [exec 140](https://rreflector.com/workflow/WyTiO3HFxigvl41t/executions/140) с двумя 🔴 от ревьюера арбитр остановил — PR #25 оставлен открытым |

Ключевые отказы, которые сценарий отработал правильно: пустой ответ модели (PR не создан,
комментарий с причиной), ревью с 🔴 major при зелёном CI (арбитраж), CI без прогона
(ветка пересоздана с CI-файлом), повторный запуск по тому же issue (старая ветка удаляется,
новый PR вместо дубля).

Известные ограничения: ревьюер 7.1 регулярно ставит 🔴 на корректные патчи — поэтому нужен
арбитр; `close issue` после мержа падает, если GitHub уже закрыл issue по `Fixes #N`
(нода терпит ошибку, статус прогона не ломается).

Скриншоты: `evidence/7.2-bug-to-master-*.png`.

## Ограничения и честные оговорки

- Gmail-нода требует OAuth-приложения Google; до подключения credential сценарий не публикуется
  (n8n валидирует credential при публикации). Критерии 6, 8, 9 подтверждаются после подключения.
- Статистика за прошлую неделю в `weekly_stats` добавлена тестовой записью (README разрешает
  сымитировать вторую неделю), остальные цифры — настоящие из GitHub API.
- Ветка `first-time-contributor` проверена повторной доставкой payload с
  `author_association = FIRST_TIME_CONTRIBUTOR` (второго реального аккаунта нет): issue #8 получил
  лейбл `first-time-contributor` и приветственный комментарий. Прогон — exec 51.
