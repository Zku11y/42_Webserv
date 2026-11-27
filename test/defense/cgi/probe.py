import json
import os
import sys

with open("marker.txt", encoding="utf-8") as marker:
    result = {
        "directory": marker.read().strip(),
        "method": os.environ["REQUEST_METHOD"],
        "query": os.environ["QUERY_STRING"],
        "body": sys.stdin.buffer.read().decode("utf-8"),
    }
print("Content-Type: application/json\r\n\r\n" + json.dumps(result))
