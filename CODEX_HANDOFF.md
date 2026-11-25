# 42 Webserv Codex handoff

I’m working on 42 Webserv v24.1 with two teammates. I own the connection handling and poll loop in [src/core/server.cpp](src/core/server.cpp) and [inc/core/server.hpp](inc/core/server.hpp). My teammates own configuration and HTTP parsing/serialization, and resource execution and CGI.

I want to get the project ready for validation. Work through one issue at a time with me in chat. Explain the behavior in plain language, show the exact insertion points, and use small C++98 changes and the existing helpers. I write the implementation myself and handle commits and pushes. Wait for explicit permission before editing project source. For the current pass, focus on project-breaking failures and leave the debug logs for later.

## Starting state

The 2026-11-25 snapshot is on `main` at `a93b3c0`, also the local `origin/main` ref when checked. At handoff, the working tree has changes in `inc/core/server.hpp`, `src/core/server.cpp`, the tracked `sr` binary, and this handoff. The source changes are my listener startup and receive-deadline fixes. The three proposed fixes below were tested only in a temporary copy. Recheck the working tree before giving advice because I may already have applied some of them.

Start with `git status --short --branch`, `git diff`, and `git log -5 --oneline`. Rebuild from the current source. The usual run is `make && ./sr default.conf`. The program takes the config path as its only argument. For an assistant-run check, a fresh binary in a temporary directory avoids overwriting the tracked `sr`:

```sh
c++ -Wall -Wextra -Werror -std=c++98 src/core/*.cpp src/http/*.cpp src/Execution/*.cpp -o /tmp/webserv-check
```

The previous checks used temporary loopback instances with disposable ports and temporary upload files. The earlier `www/poll-loop/` guide reflects an older version of the code.

### Subject constraints from the previous review

- Build with C++98 and `-Wall -Wextra -Werror`. The subject excludes external libraries and Boost.
- Use one `poll()` loop for listeners, client sockets, and CGI pipes. Sockets and pipes are nonblocking and I/O follows readiness. Regular disk files are exempt from polling.
- Respect the subject’s restrictions on `errno` after read/write, its list of allowed external functions, and its macOS-specific `fcntl()` rules if applicable. Prior notes identify `signal()` and `waitpid()` as allowed. Permission to use `setpgid()` was unverified.
- Requests need a finite deadline. Handle client disconnects and keep serving other clients.

### Behavior already checked in my source

- Startup tries all configured listeners. One failed bind leaves working listeners serving. If every bind fails, the handler returns `1` and `main()` propagates that status. A fatal `poll()` failure later in the loop still breaks to `return 0`.
- `T_OUT` is 500 ms of inactivity. `TT_OUT` is five seconds per `READ` phase, starting on accept or when the previous response finishes. The idle timer refreshes on progress. The total deadline stays fixed within that request. Tests covered a complete upload below five seconds, a finite upload over five seconds, and two requests on one socket whose combined duration exceeded five seconds.
- The poll loop streams file-backed responses. `fd_bytes_left` counts bytes still to read and `response_buffer` holds bytes still to send. EOF or a failed file read while bytes are outstanding queues client cleanup. A complete response returns the client to `READ`. A truncated request with client write EOF gets `400` and uses `WRITE_THEN_CLOSE`.
- CGI uses `CgiState::pipe_failed` and `retired_pids`. The old `cgi_pipe_errors` set is gone. Normal completion waits for stdout EOF and the direct child’s exit. The current loop can reap the child before stdout EOF. The CGI code already exports `REMOTE_ADDR` and caps headers at 16 KiB.
- The team set aside `Connection` header handling. Normal and CGI responses use `WRITE`. `WRITE_THEN_CLOSE` is still used for server-generated errors. The two tests expecting immediate closure on `Connection: close` fail because closure follows the idle timer about 500 ms later.

## Next session — three project-breaking fixes

These are the solutions from the last exchange. The line numbers refer to the snapshot above. Use the code anchors if the files shift.

### 1. Send `100 Continue` before waiting for the body

A stock `curl` upload of 2 MiB to `/cgi-bin/post_size.py` on the default config sends `Expect: 100-continue` automatically. The current server waits for the full body. Curl waits for the interim response and gets an empty reply when the server’s idle timer closes the socket. The same upload succeeds when curl sends an empty `Expect` header. The tested fix sends an interim response using the existing `POLLOUT` writer. Consult [RFC 9110 §10.1.1](https://datatracker.ietf.org/doc/html/rfc9110#section-10.1.1).

**In [inc/core/server.hpp](inc/core/server.hpp#L21)**, add `WRITE_CONTINUE,` after `WRITE,`. Add `bool continue_sent;` after `states status;` at line 94 and `continue_sent(false),` after `status(READ),` in the constructor at line 117.

**In the parser**, add `bool needs_continue() const;` among the public declarations in [inc/http/http.hpp](inc/http/http.hpp#L104). Add this definition after `get_request()` in [src/http/http.cpp](src/http/http.cpp#L11). The helper uses the parser’s existing header normalization functions and only matches while body data is still needed.

```cpp
bool HttpParser::needs_continue() const
{
    if (state == REQUEST_LINE || state == HEADERS
        || state == COMPLETE || state == ERROR)
        return false;

    std::map<std::string, std::string>::const_iterator it =
        request.headers.find("expect");
    if (it == request.headers.end())
        return false;

    const std::vector<std::string> values = split(it->second, ',');
    for (size_t i = 0; i < values.size(); ++i) {
        if (to_lower(trim_ows(values[i])) == "100-continue")
            return true;
    }
    return false;
}
```

**In [src/core/server.cpp](src/core/server.cpp#L579)**, add this `else if` immediately after the `reached_eof` block, inside the outer `READ` block. The parser-error and completed-request branches stay ahead of it, so invalid or oversized requests can still get a final error directly.

```cpp
else if (!crnt_client.continue_sent && crnt_client.parser.needs_continue()) {
    crnt_client.continue_sent = true;
    crnt_client.rsp.responc = "HTTP/1.1 100 Continue\r\n\r\n";
    crnt_client.status = WRITE_CONTINUE;
    crnt_client.t_out = get_time();
    pollfds[i].events = POLLOUT;
    continue;
}
```

**In the [write-completion switch](src/core/server.cpp#L657)**, insert the following before `case WRITE:`. Put `crnt_client.continue_sent = false;` as the first line of the existing `case WRITE:` so the next request can expect its own interim reply. The interim case keeps the parser, any buffered request bytes, and the original `tt_out` for the same request.

```cpp
case WRITE_CONTINUE:
    crnt_client.status = READ;
    pollfds[i].events = POLLIN;
    crnt_client.tw_bytes = 0;
    crnt_client.w_bytes = 0;
    crnt_client.rsp.responc.clear();
    break;
```

**In the [timeout check](src/core/server.cpp#L483)**, replace only the total-deadline condition with the following. Preserve the overall `if` closing parenthesis.

```cpp
|| ((crnt_client.status == READ || crnt_client.status == WRITE_CONTINUE)
    && now - crnt_client.tt_out > TT_OUT)
```

### 2. Shut down cleanly on a fatal `poll()` error

With a fatal `poll()` error injected during an active CGI request, Webserv exited with status `0` and the CGI kept running. Keep the `EINTR` retry. Put cleanup after the loop so both fatal `poll()` and the empty-descriptor exit go through it. `main()` already returns the handler’s status. [Poll error branch](src/core/server.cpp#L410) · [main](src/core/server.cpp#L821)

**Immediately after [`sk_close_pipe()`](src/core/server.cpp#L315)**, add:

```cpp
static void sk_retire_cgi(CgiState &cgi, std::vector<pid_t> &retired_pids)
{
    if (cgi.pid > 0 && !cgi.child_reaped) {
        kill(cgi.pid, SIGKILL);
        retired_pids.push_back(cgi.pid);
    }
    cgi.pid = -1;
}

static void sk_close_client(Client &client, CgiPipeMap &pipes,
    std::vector<pollfd> &pollfds, std::vector<pid_t> &retired_pids)
{
    sk_close_pipe(client.cgi_rsp.stdin_write_fd, pipes, pollfds);
    sk_close_pipe(client.cgi_rsp.stdout_read_fd, pipes, pollfds);
    if (client.rsp.body_fd >= 0) {
        close(client.rsp.body_fd);
        client.rsp.body_fd = -1;
    }
    sk_retire_cgi(client.cgi_rsp, retired_pids);
    sk_remove_pollfd(pollfds, client.fd);
    close(client.fd);
}
```

- In the [normal disconnect loop](src/core/server.cpp#L776), replace the block from `Client &client = found->second;` through `close(client.fd);` with `sk_close_client(found->second, pipes, pollfds, retired_pids);`. Keep `clients.erase(found);` after it.
- In the [CGI timeout and pipe-error branch](src/core/server.cpp#L697), replace the PID termination block and the following `cgi.pid = -1;` with `sk_retire_cgi(cgi, retired_pids);`.

**After the main `while` loop**, replace [`return 0;`](src/core/server.cpp#L804) with the following. The blocking waits run after the event loop has stopped. Keep `WNOHANG` in the normal loop.

```cpp
for (std::map<int, Client>::iterator it = clients.begin();
     it != clients.end(); ++it)
    sk_close_client(it->second, pipes, pollfds, retired_pids);

for (size_t i = 0; i < serverfds.size(); ++i)
    close(serverfds[i].fd);

for (size_t i = 0; i < retired_pids.size(); ++i) {
    pid_t result;
    do {
        result = waitpid(retired_pids[i], NULL, 0);
    } while (result == -1 && errno == EINTR);
}

return 1;
```

### 3. Stop CGI subprocesses on timeout or disconnect

A CGI can launch an external command that is still running after Webserv kills the CGI’s direct PID and sends `504`. A separate process group lets Webserv signal the CGI and descendants that stay in that group. Coordinate this with the CGI teammate and check the actual allowed-functions list before using `setpgid()` in the submission. If the subject excludes it, a script wrapping one external command can use `exec /path/to/program "$@"` so the current direct-PID timeout reaches that program. That workaround applies to single-command wrappers. [POSIX `setpgid()` reference](https://www.man7.org/linux/man-pages/man3/setpgid.3p.html)

**In [`CgiState`](inc/core/server.hpp#L34)**, add `pid_t pgid;` immediately after `pid_t pid;` and `pgid(-1),` after `pid(-1),` in the constructor.

**In [the child branch](src/Execution/Exec_Cgi.cpp#L184)**, put this immediately before `ft_cgi_child();`:

```cpp
if (setpgid(0, 0) == -1)
    std::exit(127);
```

**In [the parent branch](src/Execution/Exec_Cgi.cpp#L193)**, put this immediately after `client.cgi_rsp.pid = fork_fd;`. Both sides attempt group setup to cover scheduling order. The parent can accept `EACCES` because the child reaches `exec` only after successfully joining its own group.

```cpp
if (setpgid(fork_fd, fork_fd) == 0 || errno == EACCES)
    client.cgi_rsp.pgid = fork_fd;
else
    client.cgi_rsp.pipe_failed = true;
```

**In `sk_retire_cgi()` from fix 2**, add this immediately before the positive-PID `kill()`, and add `cgi.pgid = -1;` immediately after `cgi.pid = -1;`. Keep the positive PID for `waitpid()`.

```cpp
if (cgi.pgid > 0)
    kill(-cgi.pgid, SIGKILL);
```

**In the [normal CGI completion block](src/core/server.cpp#L720)**, move the two-line `if (!cgi.stdout_eof) continue;` guard from below the `waitpid()` block to immediately above it. Add `cgi.pgid = -1;` alongside `cgi.pid = -1;` inside the `ECHILD` branch, adding braces. Also clear `cgi.pgid` after setting `cgi.child_exit_status = status;` on successful reap. Waiting for stdout EOF before reaping keeps the leader’s PID reserved while a descendant can still hold stdout open and the timeout may have to signal the group.

## Verification and follow-up

- **Current checkout, before these three fixes.** The normal and ASan/UBSan runs both passed 48 of 51 checks with no sanitizer diagnostics during the run. The failures were `Expect` and the two `Connection: close` checks. The strict warning build passed. Passing cases included pipelining, an 8 MiB file checksum, interrupted fixed-length and chunked uploads, exact body and CGI output caps, concurrent static/CGI requests, and FD and direct-child cleanup. Repeated descriptor exhaustion under a temporary low FD limit recovered and CGI worked afterward.
- **Temporary copy with all three fixes.** The strict C++98 build passed. The extended regression suite passed 52 of 54 checks. The two failures were the known `Connection: close` cases. Tests confirmed that stock curl uploaded all 2,097,152 bytes, `Expect` worked twice on one socket and with chunked input, and an oversized `Expect` request got `413` directly. Injected fatal `poll()` exited with status `1` and stopped the active CGI. A CGI timeout returned `504` and stopped the test subprocess even after its parent script had exited. The project checkout still needs the three fixes.
- **Local-only test artifacts.** If the previous machine’s temporary files still exist, `/tmp/webserv-solution.QD7fZT/critical-results.log` and `/tmp/webserv-solution.QD7fZT/regression/results.log` contain the patched results. The earlier tests live in `/tmp/webserv-retest.Sp5TW1` and `/tmp/webserv-critical.PYpPvw`. Treat these paths as optional.
- **After the three fixes.** Injected CGI pipe setup and unexpected `waitpid()` errors closed the affected connection without an HTTP response. The next CGI request worked and no direct children remained. Configured custom `404` HTML worked. Server-generated `400` and timeout `504` used the generic page from `sk_generate_error()`. Leave these behind the confirmed project-breaking issues unless I ask to revisit them.
- **Earlier teammate notes.** The reason-phrase table can map unlisted statuses to `500`. The parser needs metadata limits and initialization of its embedded request’s `localPort`. File-backed response FDs may need close-on-exec. `logtime.conf` uses `server_name`, which the config parser currently rejects, and duplicate listener validation compares host strings. The `/projects` route in the default config uses a machine-specific absolute path, and `inc/main.hpp` has a glibc-specific include. Recheck these findings on the current checkout if they come into scope.
