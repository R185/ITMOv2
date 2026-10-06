# Как пользоваться локальным сетапом (практика 3)

Памятка для того, кто садится за этот ноутбук после сдачи: где что лежит, как поднять сервер,
как поговорить с моделью и как прогнать эксперименты так, чтобы результаты совпали с описанными
в `../REPORT.md`. Всё проверено на машине, на которой делалась практика.

## Что где лежит

    рабочий сервер и клиент рантайма : /var/tmp/ollama-runtime/bin/ollama   (0.34.4)
    веса и манифесты моделей         : /var/tmp/ollama-models
    профили моделей                  : lab/Modelfile (itmo-local, num_ctx 4096)
                                       lab/Modelfile.agent (itmo-agent, num_ctx 65536)
    результаты экспериментов         : lab/results/
    конфиги OpenCode                 : ./opencode.json (провайдер ollama), lab/demo/opencode.json (агент local-guide)

Каталог моделей и рантайм вынесены на раздел `/` сознательно: домашний раздел на этой машине
занят (свободно ~4 ГБ), веса занимают 3.32 ГБ, рантайм — 2.1 ГБ.

Системный `/usr/local/bin/ollama` как КЛИЕНТ рабочий: им можно делать `ollama list`, `ollama ps`,
`ollama run`. Не работает только `ollama serve` — в системной установке отсутствует движок
(`llama-server`, `libggml-cpu-*.so`), поэтому сервер поднимается бинарём из `/var/tmp/ollama-runtime`.

## Проверить, что всё живо

    curl -sS http://127.0.0.1:11434/api/version      # {"version":"0.34.4"}
    curl -sS http://127.0.0.1:11434/api/tags         # список моделей
    ollama list                                      # qwen3.5:4b, itmo-local, itmo-agent
    ollama ps                                        # что загружено: размер, CPU/GPU, контекст

## Поднять сервер (если не запущен)

    OLLAMA_MODELS=/var/tmp/ollama-models OLLAMA_NO_CLOUD=1 /var/tmp/ollama-runtime/bin/ollama serve

Держать в отдельном окне терминала. `OLLAMA_NO_CLOUD=1` отключает облачные функции Ollama,
`OLLAMA_MODELS` указывает на каталог весов.

Остановить:

    ps -o pid,args -C ollama | grep serve     # посмотреть PID
    kill <PID>

Не запускать второй сервер на 11434 — будет «address already in use». И не выполнять
`pkill -f ollama` в одной команде с текстом `ollama`: шаблон совпадёт с командной строкой
собственного шелла и убьёт сессию.

## Поговорить с моделью из терминала

Один вопрос (флаг `--think=false` обязателен, иначе модель уходит в длинную цепочку рассуждений
и отвечает минутами):

    ollama run itmo-local --think=false "Ответь одним словом: работает"

Полная модель без системного промпта курса:

    ollama run qwen3.5:4b --think=false "Сколько будет 17*23?"

Профиль для агентного режима:

    ollama run itmo-agent --think=false "Ответь: READY"

Интерактивный чат (выход — `/bye` или Ctrl+D, список команд — `/?`):

    ollama run itmo-local --think=false

Полезные флаги: `--verbose` (тайминги и ток/с), `--format json`, `--keepalive 30m`
(дольше держать модель в памяти), `--hidethinking`.

Необязательные сокращения для `~/.bashrc`:

    alias ollama-local='OLLAMA_MODELS=/var/tmp/ollama-models OLLAMA_NO_CLOUD=1 /var/tmp/ollama-runtime/bin/ollama'
    alias chat-local='ollama run itmo-local --think=false'

## Через API, без CLI

Родной API (здесь же задаются температура, seed и контекст):

    curl -sS http://127.0.0.1:11434/api/chat -d '{
      "model":"itmo-local",
      "messages":[{"role":"user","content":"Объясни разницу между моделью и сервером двумя предложениями"}],
      "stream":false,"think":false,
      "options":{"temperature":0.2,"seed":42,"num_ctx":4096,"num_predict":512}}'

OpenAI-совместимый endpoint (его использует OpenCode). Здесь рассуждения выключаются полем
`reasoning_effort`, а не флагом:

    curl -sS http://127.0.0.1:11434/v1/chat/completions -d '{
      "model":"itmo-agent",
      "messages":[{"role":"user","content":"Ответь одним словом: READY"}],
      "reasoning_effort":"none"}'

Без `reasoning_effort` тот же запрос занял 7.05 с вместо 0.62 с — это та самая грабля, из-за
которой первый агентный прогон в OpenCode не завершился.

Скорость считать из ответа: `eval_count / (eval_duration / 1e9)`; `eval_duration` — в наносекундах.

## Эксперименты скриптом

Из каталога `lab/`:

    python3 experiment.py --mode baseline --output results/my-baseline.json
    python3 experiment.py --mode system   --output results/my-system.json
    python3 experiment.py --mode system --temperature 0.8 --seed 7 --output results/my-hot7.json

Скрипт открывает выходной файл на создание и существующий не перезаписывает, поэтому свои
прогоны можно держать рядом с уже сданными. Тег модели переопределяется ключом `--model`.

## Модель в OpenCode

Разовый запуск в режиме «только чтение» (агент `local-guide`: права `read`/`glob`/`grep`,
инструкции — `lab/demo/repo-system.txt`):

    cd practices/practice_03/lab
    PATH=/home/rreflector/.opencode/bin:$PATH opencode run --dir demo \
      --agent local-guide --model ollama/itmo-agent --format json \
      "Прочитай README.md инструментом read. Назови команду тестирования со ссылкой на файл"

Интерактивная сессия Build в корне репозитория:

    cd ~/projects/ITMOv2
    PATH=/home/rreflector/.opencode/bin:$PATH opencode

Внутри: `/models` — выбрать `ollama/itmo-agent` или `ollama/itmo-local` (провайдер `ollama` уже
описан в корневом `opencode.json`), `/agent` — переключить агента. У `local-guide` запись запрещена
конфигурацией: это тестируемый помощник, а не рабочий агент.

## Чего ожидать по скорости

CPU-only (Ollama на Linux не поддерживает Intel iGPU): ~9–11 токенов в секунду на генерации,
холодная загрузка модели ~5 с, промпт-обработка ~30–35 токенов в секунду. Шаг агента в OpenCode
целиком — от минуты, потому что в контексте 64k пере-обрабатывается весь промпт.

## Мелкие грабли

- `--think=false` в CLI и `"reasoning_effort":"none"` в `/v1` — иначе долгие ответы и высокая нагрузка на CPU.
- Профили различаются контекстом: `itmo-local` — 4096, `itmo-agent` — 65536; фактический смотреть в `ollama ps`.
- Модель выгружается из памяти через 5 минут простоя, следующий запрос снова платит `load_duration`.
- `/var/tmp` может чиститься systemd по возрасту; если веса исчезли — `ollama pull qwen3.5:4b` (3.32 ГБ).
- Файлы в `lab/demo` во время сравнения не меняются: это вход эксперимента, а не песочница.
