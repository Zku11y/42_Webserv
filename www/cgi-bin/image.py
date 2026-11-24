import sys

with open("../uploads/tt.png", "rb") as f:
    data = f.read()

sys.stdout.buffer.write(b"Content-Type: image/png\r\n\r\n")
sys.stdout.buffer.write(data)