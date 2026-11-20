# Практика 7.1 — ассистент код-ревью в n8n

n8n 2.48.6, публичный адрес `https://rreflector.com`, тестовый репозиторий
[R185/itmo-ai-practice-07](https://github.com/R185/itmo-ai-practice-07).

## Состав

`workflows/7.1-pr-ai-review.json` — один сценарий, 23 ноды:

- **GitHub Webhook** (`POST /webhook/github-review`, события `pull_request` и `issue_comment`)
- **Parse Event** — определяет, что за событие: `pull_request.opened`, `/make_ai_review`, `/make_other_ai_review`
- **Choose Model** — Switch: основная модель `deepseek-flash`, вторая `deepseek-v4-pro`
- **Get PR Meta / Get PR Diff** — GitHub API, diff запрашивается с `Accept: application/vnd.github.v3.diff`
- **Split Diff into Blocks** — Code: разбирает unified diff на блоки по файлам, считает номера строк новой версии
- **AI Reviewer** — Basic LLM Chain, просит JSON вида `{summary, comments[{path,line,side,severity,body}]}`
- **Parse AI Comments** — Code: валидирует `path`/`line` против блоков diff, привязывает к реальным строкам
- **Each Comment → Post Inline Comment** — `POST /repos/{owner}/{repo}/pulls/{n}/comments` с `path`, `line`, `side`, `commit_id`
- **Post Review Summary** — `POST .../reviews` с общим текстом ревью
- **Already Reviewed? / Not Reviewed / Skip** — Data Table `reviewed_prs`, защита от повторного запуска
- **Save Reviewed PR** — пишет `pr_key`, `repo`, `pr_number`, `model`, `reviewed_at`

Дедупликация идёт по паре `pr_key + model`: повторный `/make_ai_review` к тому же PR не запускает
ревью, при этом `/make_other_ai_review` на тот же PR работает отдельной строкой.

## Критерии

Канонический пример — PR [#9](https://github.com/R185/itmo-ai-practice-07/pull/9):
открытие PR дало один прогон основной модели, `/make_other_ai_review` — один прогон второй,
повторный `/make_ai_review` — пропуск.

| # | Критерий | Баллы | Подтверждение |
|---|---|---|---|
| 1 | `/make_other_ai_review` запускает ревью второй моделью, результат в PR | 2 | PR #9, [exec 71](https://rreflector.com/workflow/jep3nyRL2cp9bgvY/executions/71): команда `/make_other_ai_review`, модель `deepseek-v4-pro`, 4 точечных комментария (строки 7, 12, 39, 51) и **одно** summary-ревью. Подпись в комментариях — `_AI review (deepseek-v4-pro)_`, у основной модели `_AI review (deepseek-flash)_`. Скриншоты: `evidence/github-pr9-conversation.png`, `evidence/github-pr9-inline-comments.png` |
| 2 | Data Table создана, id PR сохраняется после ревью | 2 | Таблица `reviewed_prs` (id `DdmNWKvt38Ahanz6`), 7 строк: PR #1, #2, #3, #9 с моделями `main`/`other`. Пишет нода **Save Reviewed PR** в тех же прогонах (exec 70, 71, 72). Скриншоты: `evidence/7.1-data-table-reviewed_prs.png`, `evidence/7.1-data-tables-list.png` |
| 3 | Повторный `/make_ai_review` к проверенному PR не запускает ревью | 3 | PR #9, [exec 72](https://rreflector.com/workflow/jep3nyRL2cp9bgvY/executions/72): в прогоне 6 нод, `Already Reviewed?` нашла строку `#9 + main`, `Skip: already reviewed` отработала, AI-ноды не запускались. То же поведение на PR #2 (exec 18) и PR #3 (exec 45). Скриншот: `evidence/7.1-dedup-skip-exec72.png` |
| 4 | Комментарии публикуются точечно, к каждому блоку изменённого кода | 5 | PR #9: 8 комментариев основной модели и 4 второй, все с `path=src/calc.py`, `line` из добавленных строк блока (7, 12, 32, 39, 51, 69, 75, 79) и `side: RIGHT`. PR #2: 8 привязок к `src/storage.py` (11, 17, 18, 22, 24, 25, 37, 38). Скриншоты: `evidence/github-pr9-inline-comments.png`, `evidence/github-pr2-two-models.png` |

Итого: 12/12 по перечисленным критериям.

## Скриншоты (`evidence/`)

| Файл | Что видно |
|---|---|
| `7.1-executions-list.png` | список запусков сценария в n8n |
| `7.1-dedup-skip-exec72.png` | прогон с пропуском: ветка `Already Reviewed? → Skip`, до AI не доходит |
| `7.1-data-tables-list.png` | обе таблицы в проекте |
| `7.1-data-table-reviewed_prs.png` | содержимое `reviewed_prs` |
| `github-pr3-inline-comments.png` | точечный комментарий, привязанный к строке diff |
| `github-pr3-both-models-conversation.png` | обсуждение PR с обеими моделями |
| `github-pr2-two-models.png` | PR #2: точечные комментарии основной и второй модели |
| `github-pr9-conversation.png` | PR #9: по одному summary-ревью от каждой модели |
| `github-pr9-inline-comments.png` | PR #9: точечные комментарии в файле |

## Ограничения и честные оговорки

- Модели работают через OpenAI-совместимый эндпоинт `https://api.deepseek.com/v1`
  (`deepseek-flash`, `deepseek-v4-pro`). Ключ VseLLM в окружении не появился, поэтому
  использован доступный ключ с той же моделью; смена base url — одна правка в credential.
- На PR #2 видно 5 одинаковых summary подряд — это баг первой версии: нода `Post Review Summary`
  получала все элементы цикла и публиковала ревью на каждый. Починено нодой `One Item for Summary`;
  на PR #3 и #9 ровно по одному summary на прогон. Старые ревью на PR #2 через API не удаляются.
- Две модели смотрят один и тот же diff, поэтому находки иногда совпадают по строкам.
  Там, где вторая модель дублировала замечание основной на той же строке, её комментарии
  удалены вручную; остались уникальные (18, 22, 37). Это косметика, не логика сценария.
- Первые прогоны (exec 12, 16) упали: шаблон промпта не подставлял блоки diff, а сохранение
  в таблицу шло по одному разу на элемент. Обе ошибки исправлены, лишние строки удалены,
  актуальные прогоны — 20, 44, 45.
