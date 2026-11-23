# 42 Webserv Codex handoff

I'm working on 42 Webserv v24.1 with two teammates. I own the connection handling and poll loop in [src/core/server.cpp](src/core/server.cpp) and [inc/core/server.hpp](inc/core/server.hpp). My teammates own configuration and HTTP parsing/serialization, and resource execution and CGI.

Start by reading those two server files and checking `git status`, `git diff`, and recent commits. Check the latest source against the snapshot below. I write the implementation myself. When I ask for a review, explain what works, identify the bugs in priority order, and use small C++98 snippets where useful. Wait for an explicit request before editing project code. Keep the design simple, reuse the existing helpers and `CgiState` fields, and avoid adding an `io_failed` flag. I handle commits and pushes.

## Subject constraints

- Use C++98 and compile with `-Wall -Wextra -Werror`. External libraries and Boost are forbidden.
- Use one `poll()` loop for readiness across listeners, client sockets, and CGI pipes. Sockets and pipes must be nonblocking. Perform readiness-driven I/O on them. Regular disk files are exempt from polling.
- Handle I/O return values without inspecting `errno` after a read or write operation. The subject lists `signal()` and `waitpid()`. Follow the subject's external function list and its macOS-specific `fcntl()` restrictions if applicable.
- Requests need a finite deadline. The server must handle client disconnection and stay available.

## CGI design we're working toward

1. `Execution::ft_handle_resource()` can start CGI. The startup code in [Exec_Cgi.cpp](src/Execution/Exec_Cgi.cpp) saves the child PID and parent pipe FDs in `client.cgi_rsp`, then sets the client to `WAIT`.
2. On entry to `WAIT`, pause ordinary socket I/O with `events = 0` and register the CGI pipes once. The pipe map is keyed by pipe FD. Each `CgiPipe` stores the direction and a reference to the `Client` in the client map.
3. Dispatch listeners first, CGI pipe entries second, and ordinary client sockets last. On stdin `POLLOUT`, write from `parsed_req.body` at `body_offset`. Close stdin after the final byte or immediately for an empty body. Independently, read stdout on `POLLIN` or `POLLHUP`, append exactly the bytes read to `raw_output`, and mark `stdout_eof` when the read returns zero.
4. Check `waitpid(..., WNOHANG)` after poll passes, including passes with no events. Track child exit separately from stdout EOF. After both are known and the child exited successfully, hand the output to the CGI finalizer. Then build the HTTP response and arm client socket `POLLOUT`.
5. Process one request per client at a time. Other clients can run CGI concurrently. After sending a keep-alive response, reset the parser and process leftover `request_buffer` bytes even if the socket has no new `POLLIN`. Before reusing `CgiState` or erasing a client, retire both pipe entries and make sure the child has been reaped or its PID is retained for reaping.

## Last verified checkpoint

At this handoff, the latest work was a working-tree change to `src/core/server.cpp` on top of `c8366c5`. Recheck the current checkout before treating these findings as open.

- The full project compiled and linked with the C++98 warning flags. Isolated pipe tests passed for empty-body registration, binary stdout capture, removing stdout from both containers at EOF, and setting `stdout_eof`. CGI was not verified end to end.
- The pipe handler closes and unregisters the current pipe directly. This works with the reverse poll loop because marking the entry as `CGI` prevents the caller from using that index as a client socket after erasure.
- `sk_setup_fd()` still reports success if `set_non_blocking()` fails. Both nonblocking setup and close-on-exec setup need to succeed.
- The CGI dispatch still ignores `sk_handle_pipes()` returning `false`. Use the existing return value for failure cleanup. A stdin entry with no ready events is normal, and `POLLNVAL` should be checked with a bitmask.
- The existing client timeout or disconnect path can erase a `WAIT` client while CGI pipe entries still reference it. Setup failure also needs to close the owned FDs and account for the child. CGI timeout handling, `SIGPIPE` handling, and nonblocking child reaping are still pending.
- Coordinate CGI response finalization with the execution teammate. The declared `ft_cgi_parent(Client&)` had no implementation at this checkpoint. If using `502` and `504`, coordinate adding them to the response builder's status table.

When I ask to continue, use the latest code and start with the smallest relevant review or implementation step. Explain how the change affects the poll loop and FD ownership.
