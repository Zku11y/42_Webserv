"use strict";

const $ = id => document.getElementById(id);
const decoder = new TextDecoder();
const sample = "Catalog fixture for the Webserv defense.\n";
const catalog = "/catalog/sample.txt";

function requireResult(condition, message) {
  if (!condition) throw new Error(message);
}

// Keep request and response evidence even when an assertion fails.
async function exchange(log, method, path, expected, body, settings = {}) {
  const options = {method, body, cache: "no-store", mode: "same-origin"};
  if (body !== undefined) {
    options.headers = {"Content-Type": typeof body === "string" ? "text/plain" : "application/octet-stream", ...settings.headers};
  }
  const controller = new AbortController();
  options.signal = controller.signal;
  const timer = setTimeout(() => controller.abort(), settings.timeout || 10000);
  log.push(method + " " + path + "  →  expect " + expected);
  for (const [key, value] of Object.entries(options.headers || {})) log.push(key + ": " + value);
  if (typeof body === "string") log.push("Request body: " + body);
  if (body instanceof Uint8Array) log.push("Request body: " + body.length + " bytes");
  if (body instanceof Blob) log.push("Request body: " + body.size + " bytes");
  try {
    const start = performance.now();
    const response = await fetch(path, options);
    log.push("HTTP " + response.status + " " + response.statusText + "  ·  " + Math.round(performance.now() - start) + " ms");
    for (const [key, value] of response.headers) log.push(key + ": " + value);
    if (settings.checkBody && response.status === expected) {
      await settings.checkBody(response, log);
      return;
    }
    const bytes = new Uint8Array(await response.arrayBuffer());
    const text = decoder.decode(bytes);
    if (!bytes.length) log.push("[empty body]");
    else if (/^(text\/|application\/json)/.test(response.headers.get("content-type") || "")) {
      log.push(text.length > 512 ? text.slice(0, 512) + " […]" : text);
    } else log.push("[" + bytes.length + " bytes]");
    log.push("");
    requireResult(response.status === expected, "Expected HTTP " + expected + ", received " + response.status + ".");
    return {response, bytes, text};
  } finally {
    controller.abort();
    clearTimeout(timer);
  }
}

async function health(log) {
  const result = await exchange(log, "GET", catalog, 200);
  requireResult(result.text === sample, "The catalog file contents did not match.");
}

async function upload(log, duplicate) {
  const file = $("payload").files[0];
  if (file && file.size > 2 * 1024 * 1024) throw new Error("Choose a file of 2 MiB or less.");
  const bytes = file ? new Uint8Array(await file.arrayBuffer()) : new Uint8Array([0, 42, 255, 13, 10, 7]);
  const id = Array.from(crypto.getRandomValues(new Uint32Array(4)), n => n.toString(16)).join("-");
  const name = "/bench-" + id + ".bin";
  const path = "/uploads" + name;
  const created = await exchange(log, "POST", path, 201, bytes);
  try {
    requireResult(created.response.headers.get("location") === path, "Expected Location to identify the new file.");
    if (duplicate) {
      await exchange(log, "POST", path, 409, bytes);
    } else {
      await exchange(log, "DELETE", "/readonly" + name, 405);
      const result = await exchange(log, "GET", path, 200);
      requireResult(result.bytes.length === bytes.length && result.bytes.every((b, i) => b === bytes[i]), "Downloaded bytes differ from the upload.");
      log.push("Matched all " + bytes.length + " bytes.\n");
    }
  } finally {
    await exchange(log, "DELETE", path, 204);
    await exchange(log, "GET", path, 404);
  }
}

async function cgi(log, method) {
  const body = method === "POST" ? "hello from webserv" : undefined;
  const result = await exchange(log, method, "/cgi/probe.py?case=defense", 200, body);
  const data = JSON.parse(result.text);
  requireResult(data.method === method && data.query === "case=defense" &&
    data.body === (body || "") && data.directory === "cgi working directory",
    "Unexpected method, query, stdin or relative file contents.");
}

function test(title, method, path, expected, run, draft = {}) {
  return {title, method, path, expected, run, draft, state: "Ready", log: []};
}

function echoCase(size, status) {
  return test("Send " + size + " bytes", "POST", "/echo",
    "The route limit is 32 bytes. Expect " + status + (status === 200 ? " and the identical body." : "."),
    async log => {
      const body = "x".repeat(size);
      const result = await exchange(log, "POST", "/echo", status, body);
      if (status === 200) requireResult(result.text === body, "Response body differs from request.");
    }, {size, status, type: "text/plain", check: status === 200 ? "match" : ""});
}

const defenseCollections = [
  {name: "HTTP basics", note: "Check method handling, error pages and continued service.", cases: [
    test("Serve a static file", "GET", catalog, "Expect 200 and the catalog fixture contents.", health),
    test("Use the custom error page", "GET", "/missing-fixture", "Expect 404 and the marker from the configured HTML file.", async log => {
      const result = await exchange(log, "GET", "/missing-fixture", 404);
      requireResult(result.text.includes("configured defense 404 page"), "Missing custom 404 marker.");
    }, {status: 404}),
    test("Reject an unknown method", "UNKNOWN", "/empty.html", "Expect 501 for an unimplemented method. A follow-up GET must still succeed.", async log => {
      try { await exchange(log, "UNKNOWN", "/empty.html", 501); }
      finally { await health(log); }
    }, {status: 501})
  ]},
  {name: "Routing", note: "Exercise the index, directory listing and redirect through the browser.", cases: [
    test("Resolve the directory index", "GET", "/", "Expect 200 and the Quick tests HTML title.", async log => {
      const result = await exchange(log, "GET", "/", 200);
      requireResult(result.text.includes("<title>Quick tests · Webserv</title>"), "Wrong directory index.");
    }),
    test("List a mapped directory", "GET", "/catalog/", "Expect 200 and a link to sample.txt in the generated index.", async log => {
      const result = await exchange(log, "GET", "/catalog/", 200);
      const doc = new DOMParser().parseFromString(result.text, "text/html");
      requireResult(!!doc.querySelector('a[href="/catalog/sample.txt"]'), "Catalog entry missing.");
    }),
    test("Follow a configured redirect", "GET", "/moved", "Expect a redirect ending at the catalog file with 200. Inspect the initial 301 in the browser Network panel.", async log => {
      const result = await exchange(log, "GET", "/moved", 200);
      requireResult(result.response.redirected && result.response.url === location.origin + catalog && result.text === sample, "Unexpected redirect target or contents.");
      log.push("Final URL: " + result.response.url);
    })
  ]},
  {name: "Body limits", note: "Compare requests just below, at and above the configured 32-byte limit.", cases: [
    echoCase(31, 200), echoCase(32, 200), echoCase(33, 413)
  ]},
  {name: "File operations", note: "Use the built-in binary sample or select a file. Each run uses its own temporary filename.", cases: [
    test("Upload, retrieve and delete", "POST / GET / DELETE", "/uploads/", "Expect 201, 405 on the read-only route, a byte-for-byte match, 204 on deletion and 404 afterward.", log => upload(log, false), {files: true}),
    test("Reject a duplicate upload", "POST", "/uploads/", "Expect 409 when a second POST uses the same filename. Remove the fixture afterward.", log => upload(log, true), {files: true})
  ]},
  {name: "CGI", note: "Check the interpreter environment, standard input and recovery from an unsuccessful child process.", cases: [
    test("Read a relative file", "GET", "/cgi/probe.py", "Expect 200, the query string, GET and the marker read from the script directory.", log => cgi(log, "GET"), {path: "/cgi/probe.py?case=defense"}),
    test("Deliver the request body", "POST", "/cgi/probe.py", "Expect 200 and the submitted text echoed from CGI stdin.", log => cgi(log, "POST"), {path: "/cgi/probe.py?case=defense", text: "hello from webserv"}),
    test("Handle a failing script", "GET", "/cgi/failure.py", "Expect a visible 502 response followed by a successful static GET.", async log => {
      try {
        const result = await exchange(log, "GET", "/cgi/failure.py", 502);
        requireResult(result.bytes.length > 0, "The error response has no body.");
      } finally { await health(log); }
    }, {status: 502})
  ]},
  {name: "Bonus", bonus: true, note: "Use after mandatory evaluation. These cases exercise Bash CGI and cookie transport.", cases: [
    test("Run the second interpreter", "GET", "/cgi/shell.sh", "Expect 200 and Bash CGI output alongside a working Python probe.", async log => {
      await cgi(log, "GET");
      const result = await exchange(log, "GET", "/cgi/shell.sh", 200);
      requireResult(result.text.trim() === "Bash CGI: GET", "Missing Bash CGI output.");
    }),
    test("Round-trip an HTTP cookie", "GET", "/cgi/cookie.py", "Issue a scoped cookie, then verify that CGI receives it on the next request.", async log => {
      await exchange(log, "GET", "/cgi/cookie.py", 200); // Clear any previous run.
      await exchange(log, "GET", "/cgi/cookie.py?issue", 200);
      const result = await exchange(log, "GET", "/cgi/cookie.py", 200);
      requireResult(result.text.trim() === "received", "CGI did not receive the cookie.");
    }, {path: "/cgi/cookie.py?issue"})
  ]}
];
