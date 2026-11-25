[Possible changes]
1. 576: might be better to send an error response instead of closing,
  even tho client closed its write side it could still recieve responses.

2. 686: might be able to replace the cgi_pipe_errors set with a simple
  flag inside Client struct.

