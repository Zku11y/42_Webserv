# 42 Webserv Codex handoff

I'm working on 42 Webserv v24.1 with two teammates. I own the connection handling and poll loop in [src/core/server.cpp](src/core/server.cpp) and [inc/core/server.hpp](inc/core/server.hpp). My teammates own configuration and HTTP parsing/serialization, and resource execution and CGI.

I want to get the project ready for validation. Work through one issue at a time in chat. Explain what happens in my code in plain language, point to the relevant block, and show a small C++98 change when useful. Reuse the existing helpers and keep the design simple. I write the implementation myself and handle commits and pushes. Wait for explicit permission before editing project code. I gave one-time permission for the `cgi_pipe_errors` change. Use this file to resume the walkthrough. The earlier `www/poll-loop/` guide is a stale snapshot.

## Starting from another checkout

This snapshot was taken on 2026-11-25. At that point `main` and the local `origin/main` ref both pointed to `4e08c24`. The changes since then were in the working tree, chiefly [server.cpp](src/core/server.cpp), [server.hpp](inc/core/server.hpp), and [response.cpp](src/http/response.cpp). Bring those edits and this handoff to the other machine. A clone of the last commit alone will be missing them. The untracked `sr` is a generated binary. Build a fresh one on the machine you use.

Start with `git status --short --branch`, `git diff`, and `git log -5 --oneline`. Read the two core files and check the current code against this snapshot. From the repository root, the default run command is `make && ./sr default.conf`. The executable requires the config path as its argument.

## Subject constraints

- Use C++98 and compile with `-Wall -Wextra -Werror`. External libraries and Boost are forbidden.
- Use one `poll()` loop for readiness across listeners, client sockets, and CGI pipes. Sockets and pipes must be nonblocking. Perform readiness-driven I/O on them. Regular disk files are exempt from polling.
- Handle I/O return values without inspecting `errno` after a read or write operation. The subject lists `signal()` and `waitpid()`. Follow the subject's external function list and its macOS-specific `fcntl()` restrictions if applicable.
- Requests need a finite deadline. The server must handle client disconnection and stay available.

## Current behavior and checks

- The current source builds with the subject warning flags. The latest localhost check fetched a 273-byte HTML file and a 793,762-byte JavaScript file byte for byte over one connection. The second request sent lowercase `Connection: close`, and the response included that header.
- The file body is read only by the poll loop. `fd_bytes_left` starts at `rsp.body_size` and counts bytes still to read. `response_buffer` holds bytes still to send. A failed read or early EOF queues socket cleanup. When the count and buffer are both empty, the poll loop closes `body_fd` and reaches normal response completion.
- In `READ`, `recv() == 0` still lets the parser process buffered bytes. EOF with no pending request queues cleanup. A truncated request builds a `400` using `WRITE_THEN_CLOSE`. Both write states can send.
- For CGI, the socket waits with ordinary events disabled. The loop handles both pipes, then checks `waitpid(..., WNOHANG)` and stdout EOF separately. Normal completion needs both. Before erasing a client or writing a CGI error, unregister the pipes and keep any unreaped child in `retired_pids`. The later sweep calls `waitpid()` to finish cleanup.
- The last CGI localhost checks returned `200` for normal CGI, `502` for output over the cap, and `504` for a script that ran past five seconds. `cgi_pipe_errors` is a set of client FDs for the current poll pass. It lets the `WAIT` loop use the timeout cleanup for pipe errors after the indexed scan ends. Find the socket's poll index after removing the pipes.
- CGI already exports `REMOTE_ADDR` from the IP captured by `accept()`. The CGI finalizer also has a separate 16 KiB header cap and checks for the earliest header terminator. [Exec_Cgi.cpp](src/Execution/Exec_Cgi.cpp)

## Next steps in my part

1. **Finish `Connection: close`.** The ordinary response path after the CGI `WAIT` branch currently handles the literal lowercase value `close`. Extend the check to a case-insensitive comma-separated token and reuse it before building a successful CGI response. The request header map uses lowercase keys and preserves the value's case. Use `WRITE_THEN_CLOSE` and set the response header before `builder.build()`. Check ordinary replies, CGI replies, and two requests on a keep-alive socket.
2. **Finish CGI error handling.** `sk_add_pipes()` failure and unexpected `waitpid()` failure still close the socket. If it is still live, route them through the existing CGI cleanup and error response. Keep the `EINTR` retry and `ECHILD` handling. Move the output-cap check before appending a new chunk.
3. **Initialize defaults and fix startup errors.** Some `Client`, nested parser request, and response scalars are copied before initialization. Initialize them at construction, using `-1` for an unused FD. Change `fd_bytes_left` to `size_t` to match `rsp.body_size`. For listeners, use the unsigned IP from `ServerConfig::getIp()` and make startup report failure when it cannot listen. Propagate the connection loop's return status from `main()`.
4. **Check deadlines and cleanup.** The 500 ms socket timer is an idle timer that resets on successful reads. Add a separate receive deadline if needed for the subject. Before submission, check slow and concurrent clients, disconnects during CGI, descriptor exhaustion, and repeated CGI cleanup for FD leaks and zombies. Full request-body debug logging is worth trimming for these runs.

## Remaining teammate work

- **HTTP and config.** The response builder rewrites codes missing from its reason-phrase table to `500`. This includes `409`, `415`, and CGI `202`. Parser limits for request lines, headers and chunk metadata, and support for `Expect: 100-continue`, are still open. `logtime.conf` uses `server_name`, which the config parser currently rejects. Duplicate listener validation compares host strings. Use a numeric endpoint comparison if padded IPv4 spellings must be treated alike. [response.cpp](src/http/response.cpp) · [http.cpp](src/http/http.cpp) · [config.cpp](src/http/config.cpp) · [configParser.cpp](src/http/configParser.cpp)
- **Resource execution and CGI.** Coordinate close-on-exec for file-backed responses, escaping and URL encoding for autoindex and redirects, consistent upload mapping when `upload_store` differs from `root`, and CGI/static/writable directory boundaries. Repeated CGI `Set-Cookie` headers still collapse to one map entry if cookie support is in scope. [Exec_Rsp.cpp](src/Execution/Exec_Rsp.cpp) · [Exec_Cgi.cpp](src/Execution/Exec_Cgi.cpp)
- **Different machines.** The default `/projects` route uses a machine-specific absolute root, and the common header includes a glibc-specific header. Check both when moving the project, especially for a macOS build. [default.conf](default.conf) · [main.hpp](inc/main.hpp)
