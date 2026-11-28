#!/usr/bin/env python3
"""TM-04 検証用の偽 SwitchBot API v1.1 サーバー

使い方:
  python scripts/fake_switchbot_server.py --scenario 1
  --scenario 1: 命令受付後 6 秒で moving=false になり目標位置へ到達（成功・確認済み）
  --scenario 2: HTTP 401 認証エラー
  --scenario 3: HTTP 200 で statusCode 190（ハブ・機器未応答）
  --scenario 4: 20 秒応答遅延（ネットワークタイムアウト）
  --scenario 5: 命令成功するが moving=true のまま（45 秒後に未確認）
  --scenario 6: 1回目・2回目の命令は失敗し、3回目で成功（15秒・60秒後再試行の確認）
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import threading
import time


class FakeState:
    def __init__(self, scenario: int) -> None:
        self.scenario = scenario
        self.lock = threading.Lock()
        self.slide_position = 40  # SwitchBot: 0=全開, 100=全閉
        self.target_position = 40
        self.moving_until = 0.0
        self.command_attempts = 0


def make_handler(state: FakeState):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            # トークン等のヘッダー値は一切出力しない
            print(f"[FakeSwitchBot] {self.command} {self.path} -> {fmt % args}")

        def send_json(self, http_code: int, payload: dict) -> None:
            raw = json.dumps(payload, ensure_ascii=False).encode("utf-8")
            self.send_response(http_code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

        def check_common_scenario(self) -> bool:
            if state.scenario == 2:
                self.send_json(401, {"message": "Unauthorized"})
                return True
            if state.scenario == 3:
                self.send_json(200, {"statusCode": 190, "message": "Device internal error"})
                return True
            if state.scenario == 4:
                time.sleep(20.0)
                self.send_json(200, {"statusCode": 100, "body": {}})
                return True
            return False

        def do_GET(self):
            if self.check_common_scenario():
                return

            if self.path.endswith("/devices"):
                self.send_json(
                    200,
                    {
                        "statusCode": 100,
                        "message": "success",
                        "body": {
                            "deviceList": [
                                {
                                    "deviceId": "CURTAIN3-001",
                                    "deviceName": "寝室カーテン",
                                    "deviceType": "Curtain3",
                                    "enableCloudService": True,
                                    "hubDeviceId": "HUB2-001",
                                },
                                {
                                    "deviceId": "HUB2-001",
                                    "deviceName": "ハブ2",
                                    "deviceType": "Hub 2",
                                    "enableCloudService": True,
                                    "hubDeviceId": "",
                                },
                            ]
                        },
                    },
                )
                return

            if self.path.endswith("/status"):
                with state.lock:
                    now = time.monotonic()
                    if state.scenario == 5:
                        moving = True
                        pos = state.slide_position
                    elif now < state.moving_until:
                        moving = True
                        pos = state.slide_position
                    else:
                        moving = False
                        state.slide_position = state.target_position
                        pos = state.slide_position

                self.send_json(
                    200,
                    {
                        "statusCode": 100,
                        "message": "success",
                        "body": {
                            "deviceId": "CURTAIN3-001",
                            "deviceType": "Curtain3",
                            "slidePosition": pos,
                            "moving": moving,
                        },
                    },
                )
                return

            self.send_json(404, {"statusCode": 404, "message": "Not Found"})

        def do_POST(self):
            length = int(self.headers.get("Content-Length", "0"))
            raw_body = self.rfile.read(length).decode("utf-8") if length > 0 else "{}"

            if self.check_common_scenario():
                return

            if self.path.endswith("/commands"):
                data = json.loads(raw_body)
                cmd = data.get("command", "")
                param = str(data.get("parameter", ""))

                with state.lock:
                    state.command_attempts += 1
                    if state.scenario == 6 and state.command_attempts < 3:
                        self.send_json(200, {"statusCode": 190, "message": "Temporary hub failure"})
                        return

                    if cmd == "turnOn":
                        state.target_position = 0
                    elif cmd == "turnOff":
                        state.target_position = 100
                    elif cmd == "setPosition":
                        parts = param.split(",")
                        if len(parts) == 3:
                            state.target_position = max(0, min(100, int(parts[2])))

                    state.moving_until = time.monotonic() + 6.0

                self.send_json(200, {"statusCode": 100, "message": "success", "body": {}})
                return

            self.send_json(404, {"statusCode": 404, "message": "Not Found"})

    return Handler


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=18080)
    parser.add_argument("--scenario", type=int, default=1, choices=[1, 2, 3, 4, 5, 6])
    args = parser.parse_args()

    state = FakeState(args.scenario)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), make_handler(state))
    print(f"Fake SwitchBot v1.1 listening on [http://127.0.0.1](http://127.0.0.1):{args.port}/v1.1 (scenario={args.scenario})")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
