(() => {
  "use strict";

  const trace = document.getElementById("cgi-trace");
  if (!trace) return;

  const scenario = document.getElementById("trace-scenario");
  const previous = document.getElementById("trace-prev");
  const next = document.getElementById("trace-next");
  const progress = document.getElementById("trace-progress");
  const fields = [
    "title", "detail", "socket", "pipes", "input", "output", "child", "queues"
  ];
  const displays = fields.map((field) => document.getElementById("trace-" + field));
  if (!scenario || !previous || !next || !progress || displays.some((item) => !item)) {
    return;
  }

  const started = {
    title: "Register the CGI pipes",
    detail: "Execution stores the child PID and pipe descriptors. The server pauses ordinary socket I/O and registers both pipe ends for this example POST.",
    socket: "FD 7 · WAIT · events = 0",
    pipes: "FD 10 · POLLOUT / FD 11 · POLLIN",
    input: "0 of 12 bytes sent",
    output: "0 bytes · EOF pending",
    child: "PID 421 · running",
    queues: "Empty"
  };

  const fed = {
    ...started,
    title: "Finish writing stdin",
    detail: "After writing the final body bytes, the handler closes FD 10 and removes it from the pipe map and poll list.",
    pipes: "FD 11 · POLLIN",
    input: "12 of 12 bytes sent · stdin closed"
  };

  const interleaved = {
    ...started,
    title: "Read stdout while stdin is still open",
    detail: "One write accepted five body bytes. The script has already produced some output, so the server can read it now and resume the body write at byte five when stdin is writable.",
    input: "5 of 12 bytes sent · body_offset = 5",
    output: "26 bytes · EOF pending"
  };

  const draining = {
    ...fed,
    title: "Read buffered stdout",
    detail: "The pipe can contain data even when it reports POLLHUP. A positive read appends exactly the returned byte count to raw_output.",
    output: "29 bytes · EOF pending",
    child: "PID 421 · exit not yet collected"
  };

  const ready = {
    ...draining,
    title: "Build the 200 response",
    detail: "A later stdout read returns 0 and the loop closes FD 11. After waitpid collects a successful exit, ft_cgi_finish parses the output and the socket is armed for writing. EOF and reaping can arrive in either order.",
    socket: "FD 7 · WRITE · POLLOUT",
    pipes: "None",
    output: "EOF seen · output parsed",
    child: "PID 421 · reaped"
  };

  const sent = {
    ...ready,
    title: "Send and return to reading",
    detail: "When all response bytes have been sent, the server resets the request parser and puts the socket back in READ.",
    socket: "FD 7 · READ · POLLIN"
  };

  const pipeError = {
    ...fed,
    title: "Record a pipe error",
    detail: "The stdout handler reports failure. The reverse scan records the owning socket in cgi_pipe_errors and leaves request-level cleanup to the WAIT sweep.",
    pipes: "FD 11 · error reported",
    queues: "cgi_pipe_errors contains FD 7"
  };

  const timeout = {
    ...fed,
    title: "The CGI deadline passes",
    detail: "The WAIT sweep finds that more than five seconds have elapsed since CGI started. It chooses 504 even if the last poll returned no events.",
    child: "PID 421 · still running after 5 s"
  };

  const errorReply = (code) => ({
    ...fed,
    title: "Build the " + code + " response",
    detail: "The WAIT sweep removes both pipes, signals the still-running child, and records its PID for reaping. It then finds the socket again and arms the error response for writing.",
    socket: "FD 7 · WRITE_THEN_CLOSE · POLLOUT",
    pipes: "None",
    output: "Capture stopped · " + code + " prepared",
    child: "PID 421 · SIGKILL sent",
    queues: "retired_pids tracks PID 421 until reaped"
  });

  const errorSent = (code) => ({
    ...errorReply(code),
    title: "Send the error and close",
    detail: "After sending the error, the write state queues FD 7 for cleanup. The reaping sweep separately removes PID 421 once waitpid collects it.",
    socket: "FD 7 · closed",
    output: code + " sent · Connection: close",
    child: "PID 421 · reaped",
    queues: "Cleanup complete"
  });

  const disconnect = {
    ...started,
    title: "Queue the disconnected client",
    detail: "A fatal socket event marks FD 7 for cleanup. The WAIT sweep skips clients already in clients_to_close.",
    socket: "FD 7 · cleanup pending",
    queues: "clients_to_close contains FD 7"
  };

  const disconnected = {
    ...disconnect,
    title: "Release descriptors and retire the child",
    detail: "The cleanup sweep removes both CGI pipes, signals the child, then closes and removes the client socket. A reaping sweep keeps checking for the child's exit.",
    socket: "FD 7 · closed",
    pipes: "None",
    output: "Capture stopped",
    child: "PID 421 · SIGKILL sent",
    queues: "retired_pids tracks PID 421 until reaped"
  };

  const reaped = {
    ...disconnected,
    title: "Reap the child",
    detail: "The reaping sweep removes the PID from retired_pids when waitpid collects the exit.",
    child: "PID 421 · reaped",
    queues: "Cleanup complete"
  };

  const scenarios = {
    success: [
      started,
      interleaved,
      { ...fed, output: interleaved.output },
      draining,
      ready,
      sent
    ],
    "pipe-error": [started, fed, pipeError, errorReply(502), errorSent(502)],
    timeout: [started, fed, timeout, errorReply(504), errorSent(504)],
    disconnect: [started, disconnect, disconnected, reaped]
  };

  let current = 0;
  let steps = scenarios[scenario.value] || scenarios.success;

  function render() {
    const step = steps[current];
    progress.textContent = "Step " + (current + 1) + " of " + steps.length;
    fields.forEach((field, index) => {
      displays[index].textContent = step[field];
    });
    previous.disabled = current === 0;
    next.disabled = current === steps.length - 1;
  }

  scenario.addEventListener("change", () => {
    steps = scenarios[scenario.value] || scenarios.success;
    current = 0;
    render();
  });

  previous.addEventListener("click", () => {
    if (current > 0) current -= 1;
    render();
  });

  next.addEventListener("click", () => {
    if (current < steps.length - 1) current += 1;
    render();
  });

  render();
  trace.removeAttribute("hidden");
  trace.querySelectorAll("[hidden]").forEach((item) => item.removeAttribute("hidden"));
})();
