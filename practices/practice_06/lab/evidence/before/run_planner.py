"""Run the shop-planner prompt against the local model; save the raw answer."""
import json
import sys
import urllib.request

ctx_path, out_path = sys.argv[1], sys.argv[2]
system = open("/home/rreflector/projects/ITMOv2/practices/practice_06/planner-system.txt", encoding="utf-8").read()
context = open(ctx_path, encoding="utf-8").read()
body = json.dumps({
    "model": "itmo-agent",
    "stream": False,
    "think": False,
    "format": "json",
    "options": {"temperature": 0, "num_ctx": 8192, "num_predict": 256},
    "messages": [
        {"role": "system", "content": system},
        {"role": "user", "content": context},
    ],
}).encode()
req = urllib.request.Request("http://127.0.0.1:11434/api/chat", data=body,
                             headers={"Content-Type": "application/json"})
with urllib.request.urlopen(req, timeout=600) as r:
    data = json.load(r)
answer = data["message"]["content"]
open(out_path, "w", encoding="utf-8").write(answer)
print("ANSWER:", answer)
print("STATS:", {k: data.get(k) for k in ("total_duration", "eval_count", "prompt_eval_count")})
