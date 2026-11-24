import sys

body = sys.stdin.buffer.read()

print("Content-Type: text/plain")
print()
print("Received bytes:", len(body))