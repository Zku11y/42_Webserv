from http.cookies import SimpleCookie
import os

name = "bench_cookie"
cookies = SimpleCookie(os.environ.get("HTTP_COOKIE", ""))
issue = os.environ.get("QUERY_STRING") == "issue"
print("Content-Type: text/plain")
if issue:
    print("Set-Cookie: bench_cookie=received; Path=/cgi/cookie.py; HttpOnly; SameSite=Strict")
else:
    print("Set-Cookie: bench_cookie=; Max-Age=0; Path=/cgi/cookie.py; HttpOnly; SameSite=Strict")
print()
if issue:
    print("issued")
elif name in cookies:
    print(cookies[name].value)
else:
    print("missing")
