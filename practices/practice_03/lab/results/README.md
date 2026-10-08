# Каталог результатов: что здесь чьё

Файлы в этом каталоге появились из трёх разных источников, и их стоит различать: ответы
принадлежат скачанной модели, эталоны и отчёты написаны исполнителем, часть файлов — просто
сохранённый вывод команд. Ниже — карта авторства и команды, которые воспроизводят каждый артефакт.

## Сгенерировано моделью `qwen3.5:4b`

| Файл | Что внутри | Чем создан |
|---|---|---|
| `first-run.json` | сырой ответ Ollama: `message.content` (текст модели) + метрики `total_duration`, `load_duration`, `prompt_eval_count`, `eval_count`, `eval_duration` | `curl /api/chat` |
| `baseline.json` | ответ без системного промпта; ключи `request`, `response`, `wall_seconds`, `load_seconds`, `total_seconds`, `decode_tokens_per_second` | `experiment.py --mode baseline` |
| `system.json` | ответ с системным промптом (`request.messages[0].role == "system"`) | `experiment.py --mode system` |
| `hot42.json`, `hot43.json`, `hot44.json` | ответы при `temperature=0.8`, сиды 42/43/44 | `experiment.py --mode system --temperature 0.8 --seed N` |
| `cold42.json`, `cold43.json`, `cold44.json` | ответы при `temperature=0.2`, те же сиды | `experiment.py --mode system --temperature 0.2 --seed N` |
| `repeat1.json` … `repeat3.json` | прогретые повторы одного запроса для медианы скорости | `experiment.py --mode system --seed 42` |
| `q1.jsonl` … `q5.jsonl` | ответ на каждый из пяти вопросов `lab/QUESTIONS.md` + события сессии OpenCode (`step_start`, `tool_use`, `step_finish`, `text`); внутри `tool_use` — фактический вызов инструмента со статусом | `opencode run --dir demo --agent local-guide --model ollama/itmo-agent` |
| `read-check.jsonl` | ответ после реального вызова `read` по `demo/README.md` | то же, отдельный запуск |

Сами файлы записаны скриптами (`experiment.py`, `opencode run`), но всё содержательное —
формулировки ответов, решения о вызове инструментов, числа токенов — произведено моделью.

## Написано исполнителем (модель этот текст не видела)

| Файл | Что внутри |
|---|---|
| `../REPORT.md` | отчёт по практике: сетап, модель, эксперименты, разбор ошибок и ограничений |
| `../lab/HOWTO-local-model.md` | памятка: как поднять сервер, выключить режим рассуждений, повторить прогон |
| `reference_answers.md` | эталоны для пяти вопросов, собранные по коду `demo/` с хешами источников |
| `ollama-ps.txt` | сохранённый вывод `ollama ps` (объём в памяти, процессор, длина контекста) |
| `evidence-opencode-timeout.log`, `evidence-opencode-first-attempt.txt` | вывод неудачных прогонов до исправления настроек, приведён как свидетельство проблемы |
| `../../../../opencode.json` (корень репозитория) | добавлен провайдер `ollama` и модели `itmo-local`, `itmo-agent` |
| `../../demo/opencode.json` | добавлена опция `reasoningEffort: none` для агента `local-guide` |

Важно про `reference_answers.md`: эталоны не передавались тестируемой модели ни в одном прогоне —
иначе сравнение её ответов с эталоном не имело бы смысла.

## Не изменялось (входные материалы курса)

`lab/Modelfile`, `lab/Modelfile.agent`, `lab/system.txt`, `lab/experiment.py`, `lab/QUESTIONS.md`,
а также все файлы `lab/demo/`: `README.md`, `repo-system.txt`, `service.py`, `test_service.py`,
`Makefile`, `.gitignore`. Из `lab/Modelfile` и `lab/Modelfile.agent` собраны профили `itmo-local`
и `itmo-agent` командой `ollama create` — правок в сами файлы не вносилось.

## Вне репозитория (в git не попадает)

    /var/tmp/ollama-runtime   рабочий сервер Ollama 0.34.4, распакован из официального архива
    /var/tmp/ollama-models    веса qwen3.5:4b (3.32 ГБ) и собранные из Modelfile профили

Каталоги вынесены на раздел `/` из-за нехватки места на домашнем разделе; исходный архив
`ollama-runtime.tar.zst` после распаковки удалён.

## Как отличить самому

    python3 -m json.tool baseline.json | head -30     # видно response.message.content — текст модели
    grep -o '"tool":"[a-z]*"' q1.jsonl                 # фактические вызовы инструментов
    grep -c '"type":"tool_use"' q*.jsonl               # сколько шагов с инструментами в каждом вопросе
