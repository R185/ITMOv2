"""Regression tests for the attacks from REPORT_01: each one fails on the original code."""

import json
import threading
import unittest
from http.server import ThreadingHTTPServer
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from service import Shop, make_handler


class SecurityTest(unittest.TestCase):
    def setUp(self):
        self.shop = Shop()
        self.addCleanup(self.shop.close)

    def request(self, method, path, body=None, headers=None):
        with self.shop.lock:
            return self.shop.dispatch(method, path, body or {}, headers or {})

    def login(self, username="alice", password=None):
        password = password or {"alice": "learn-alice", "bob": "learn-bob",
                                "staff": "learn-staff"}[username]
        status, result = self.request("POST", "/login",
                                      {"username": username, "password": password})
        self.assertEqual(status, 200)
        return {"X-Token": result["token"]}

    # --- SQL injection ---------------------------------------------------------

    def test_profile_injection_is_rejected(self):
        auth = self.login()
        self.assertEqual(self.request("GET", "/profile?id=0 OR 1=1", headers=auth)[0], 400)
        self.assertEqual(self.request("GET", "/profile?id=1;DROP TABLE profiles", headers=auth)[0], 400)

    def test_profile_of_another_customer_is_hidden(self):
        self.assertEqual(self.request("GET", "/profile?id=2", headers=self.login())[0], 404)
        self.assertEqual(self.request("GET", "/profile?id=2",
                                      headers=self.login("staff"))[1][0]["name"], "TEST_BOB")

    def test_search_injection_returns_only_own_profile(self):
        auth = self.login()
        status, rows = self.request("GET", "/search?q=%27+OR+%271%27%3D%271", headers=auth)
        self.assertEqual(status, 200)
        self.assertNotIn(2, [row["id"] for row in rows])
        status, rows = self.request("GET", "/search?q=TEST", headers=auth)
        self.assertEqual([row["id"] for row in rows], [1])
        status, rows = self.request("GET", "/search?q=TEST", headers=self.login("staff"))
        self.assertEqual([row["id"] for row in rows], [1, 2])

    def test_assistant_context_injection_is_rejected(self):
        auth = self.login()
        self.assertEqual(self.request("GET", "/assistant/context?order_id=0 OR 1=1",
                                      headers=auth)[0], 400)
        self.assertEqual(self.request("GET", "/assistant/context?order_id=2", headers=auth)[0], 404)
        self.assertEqual(self.request("GET", "/assistant/context?order_id=1", headers=auth)[0], 200)

    # --- authentication and authorization --------------------------------------

    def test_customer_routes_require_a_session(self):
        for path in ("/profile", "/orders", "/search?q=TEST", "/files",
                     "/assistant/context?order_id=1", "/reviews"):
            with self.subTest(path=path):
                status = self.request("GET" if path != "/reviews" else "POST", path,
                                      {"product": "chair", "text": "x"})[0]
                self.assertEqual(status, 401)

    def test_orders_are_scoped_to_the_session(self):
        auth = self.login()
        status, orders = self.request("GET", "/orders", headers=auth)
        self.assertEqual(status, 200)
        self.assertEqual([order["owner"] for order in orders], [1])
        status, orders = self.request("GET", "/orders", headers=self.login("staff"))
        self.assertEqual(len(orders), 2)

    def test_role_header_does_not_grant_staff(self):
        auth = {**self.login(), "X-Role": "staff"}
        self.assertEqual(self.request("GET", "/admin/export", headers=auth)[0], 403)
        self.assertEqual(self.request("GET", "/admin/export", headers=self.login("staff"))[0], 200)

    def test_files_are_limited_to_the_owner_directory(self):
        auth = self.login()
        self.assertEqual(self.request("GET", "/files?user=bob&path=receipt.txt", headers=auth)[0], 403)
        self.assertEqual(self.request(
            "GET", "/files?path=../../private/operator-note.txt", headers=auth)[0], 403)
        status, own = self.request("GET", "/files?user=alice&path=receipt.txt", headers=auth)
        self.assertEqual(status, 200)
        self.assertIn("TEST_ALICE", own["content"])
        status, other = self.request("GET", "/files?user=bob&path=receipt.txt",
                                     headers=self.login("staff"))
        self.assertEqual(status, 200)
        self.assertIn("TEST_BOB", other["content"])

    def test_assistant_execute_requires_session_and_permissions(self):
        decision = {"decision": {"tool": "read_file",
                                 "arguments": {"path": "files/bob/receipt.txt"}}}
        self.assertEqual(self.request("POST", "/assistant/execute", decision)[0], 401)
        auth = self.login()
        self.assertEqual(self.request("POST", "/assistant/execute", decision, auth)[0], 403)
        self.assertEqual(self.request("POST", "/assistant/execute", {
            "decision": {"tool": "export_profiles", "arguments": {}}}, auth)[0], 403)
        self.assertEqual(self.request("POST", "/assistant/execute", {
            "decision": {"tool": "delete_tree", "arguments": {"path": "files"}}}, auth)[0], 403)
        status, own = self.request("POST", "/assistant/execute", {
            "decision": {"tool": "read_file",
                         "arguments": {"path": "files/alice/receipt.txt"}}}, auth)
        self.assertEqual(status, 200)
        self.assertIn("TEST_ALICE", own["content"])

    def test_staff_delete_stays_inside_a_user_directory(self):
        staff = self.login("staff")
        self.assertEqual(self.request("POST", "/assistant/execute", {
            "decision": {"tool": "delete_tree", "arguments": {"path": "."}}}, staff)[0], 403)
        self.assertEqual(self.request("POST", "/assistant/execute", {
            "decision": {"tool": "delete_tree", "arguments": {"path": "files"}}}, staff)[0], 403)
        self.assertEqual(self.request("POST", "/assistant/execute", {
            "decision": {"tool": "delete_tree",
                         "arguments": {"path": "files/alice/receipt.txt"}}}, staff)[0], 200)
        self.assertEqual(self.request("GET", "/files?path=receipt.txt", headers=self.login())[0], 404)

    # --- business logic --------------------------------------------------------

    def test_price_cannot_be_overridden(self):
        auth = self.login()
        status, order = self.request("POST", "/orders",
                                     {"product": "chair", "price": 1}, auth)
        self.assertEqual((status, order["price"]), (201, 5000))
        status, order = self.request("POST", "/orders",
                                     {"product": "lamp", "price": -999999}, auth)
        self.assertEqual((status, order["price"]), (201, 1500))

    # --- output encoding and secrets -------------------------------------------

    def test_review_html_is_escaped(self):
        auth = self.login()
        payload = "<img src=x onerror=alert(1)>"
        self.assertEqual(self.request("POST", "/reviews",
                                      {"product": "chair", "text": payload}, auth)[0], 201)
        status, page = self.request("GET", "/reviews/view")
        self.assertEqual(status, 200)
        self.assertNotIn(payload, page)
        self.assertIn("&lt;img", page)

    def test_debug_logs_are_staff_only_and_redacted(self):
        alice = self.login()
        self.request("POST", "/orders", {"product": "lamp"}, alice)
        self.assertEqual(self.request("GET", "/debug/logs", headers=alice)[0], 403)
        status, logs = self.request("GET", "/debug/logs", headers=self.login("staff"))
        self.assertEqual(status, 200)
        dumped = json.dumps(logs, ensure_ascii=False)
        self.assertNotIn("learn-alice", dumped)
        self.assertNotIn(alice["X-Token"], dumped)
        self.assertIn("<redacted>", dumped)

    def test_errors_do_not_leak_internals(self):
        status, error = self.request("GET", "/profile?id=abc", headers=self.login())
        self.assertEqual(status, 400)
        self.assertNotIn("SELECT", json.dumps(error))
        status, error = self.request("POST", "/orders", {}, self.login())
        self.assertEqual(status, 400)
        self.assertNotIn("KeyError", json.dumps(error))

    # --- sessions ---------------------------------------------------------------

    def test_login_is_rate_limited(self):
        for _ in range(5):
            status = self.request("POST", "/login",
                                  {"username": "alice", "password": "wrong"})[0]
            self.assertEqual(status, 401)
        self.assertEqual(self.request("POST", "/login",
                                      {"username": "alice", "password": "wrong"})[0], 429)
        self.assertEqual(self.request("POST", "/login",
                                      {"username": "alice", "password": "learn-alice"})[0], 429)

    def test_logout_revokes_the_token(self):
        auth = self.login()
        self.assertEqual(self.request("POST", "/logout", headers=auth)[0], 200)
        self.assertEqual(self.request("GET", "/orders", headers=auth)[0], 401)

    def test_expired_session_is_rejected(self):
        auth = self.login()
        token = auth["X-Token"]
        self.shop.sessions[token]["created"] -= 4000
        self.assertEqual(self.request("GET", "/orders", headers=auth)[0], 401)


class HttpHeadersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.shop = Shop()
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(cls.shop))
        cls.server.daemon_threads = True
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = f"http://127.0.0.1:{cls.server.server_port}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.thread.join(timeout=5)
        cls.server.server_close()
        cls.shop.close()

    def call(self, method, path, body=None, headers=None):
        headers = {"Content-Type": "application/json", **(headers or {})}
        data = json.dumps(body).encode() if body is not None else None
        request = Request(self.base + path, data=data, headers=headers, method=method)
        try:
            response = urlopen(request, timeout=5)
        except HTTPError as exc:
            response = exc
        with response:
            return response.status, response.headers, response.read().decode()

    def test_html_has_csp_and_no_script(self):
        status, _, _ = self.call("POST", "/login", {"username": "alice",
                                                    "password": "learn-alice"})
        self.assertEqual(status, 200)
        status, headers, body = self.call("GET", "/reviews/view")
        self.assertEqual(status, 200)
        self.assertIn("default-src 'none'", headers["Content-Security-Policy"])
        self.assertNotIn("<script", body)

    def test_invalid_json_is_a_client_error(self):
        request = Request(self.base + "/login", data=b"{oops",
                          headers={"Content-Type": "application/json"}, method="POST")
        with self.assertRaises(HTTPError) as caught:
            urlopen(request, timeout=5)
        self.assertEqual(caught.exception.code, 400)
        body = json.loads(caught.exception.read().decode())
        self.assertEqual(body, {"error": "invalid json"})

    def test_wrong_content_type_is_rejected(self):
        request = Request(self.base + "/login", data=b'{"username":"alice"}',
                          headers={"Content-Type": "text/plain"}, method="POST")
        with self.assertRaises(HTTPError) as caught:
            urlopen(request, timeout=5)
        self.assertEqual(caught.exception.code, 415)


if __name__ == "__main__":
    unittest.main()
