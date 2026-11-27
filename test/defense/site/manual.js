"use strict";

const intra = location.pathname.startsWith("/bench/");
const secondary = location.pathname.startsWith("/tools/");
const presets = new Map();
let activeRequest;
let downloadUrl;

function addPresets(name, cases) {
  const group = document.createElement("optgroup");
  group.label = name;
  for (const item of cases) {
    if (item.draft.files) continue;
    presets.set(item.title, item);
    group.append(new Option(item.title, item.title));
  }
  if (group.children.length) $("preset").append(group);
}

function updateBodyFields() {
  const method = $("method").value.trim().toUpperCase();
  const bodyAllowed = method !== "GET" && method !== "HEAD";
  $("body-fields").hidden = !bodyAllowed;
  $("body-fields").disabled = !bodyAllowed;
  const kind = $("body-kind").value;
  $("text-fields").hidden = kind !== "text";
  $("size-fields").hidden = kind !== "generated";
  $("raw-file-fields").hidden = kind !== "file";
  $("byte-count").disabled = kind !== "generated";
}

function loadPreset() {
  const item = presets.get($("preset").value);
  const draft = item?.draft || {};
  $("method").value = item?.method || "GET";
  $("path").value = draft.path || item?.path || "/";
  $("expected").value = item ? draft.status || 200 : "";
  $("headers").value = [
    draft.type ? "Content-Type: " + draft.type : "",
    draft.headers || ""
  ].filter(Boolean).join("\n");
  $("body-kind").value = draft.size !== undefined ? "generated" : draft.text !== undefined ? "text" : "none";
  $("body-text").value = draft.text || "";
  $("byte-count").value = draft.size ?? 32;
  $("body-check").value = draft.check || "";
  updateBodyFields();
}

function readHeaders() {
  const headers = new Headers();
  for (const line of $("headers").value.split("\n")) {
    if (!line.trim()) continue;
    const colon = line.indexOf(":");
    if (colon < 1) throw new Error("Enter each request header as Name: value.");
    headers.append(line.slice(0, colon).trim(), line.slice(colon + 1).trim());
  }
  return headers;
}

function composeRequest() {
  const path = $("path").value.trim();
  const url = new URL(path, location.origin);
  if (!path.startsWith("/") || url.origin !== location.origin || url.hash) {
    throw new Error("Enter a path on this listener, beginning with / and without a URL fragment.");
  }
  const method = $("method").value.trim().toUpperCase();
  const headers = readHeaders();
  let body;
  if (!$("body-fields").disabled) {
    switch ($("body-kind").value) {
      case "text":
        body = new Blob([$("body-text").value], {type: "text/plain;charset=UTF-8"});
        break;
      case "generated":
        body = repeatedBody($("byte-count").valueAsNumber);
        break;
      case "file":
        body = $("raw-file").files[0];
        if (!body) throw new Error("Choose a body file.");
        break;
    }
  }
  if (body && !headers.has("Content-Type")) headers.set("Content-Type", body.type || "application/octet-stream");
  const check = $("body-fields").disabled ? "" : $("body-check").value;
  if (check === "digit" && !/^\d$/.test(headers.get("X-Secret-Header-For-Test") || "")) {
    throw new Error("Set X-Secret-Header-For-Test to one digit for this comparison.");
  }
  return {method, path: url.pathname + url.search, headers, body, check, expected: $("expected").value};
}

function fileRequest(method) {
  const name = $("remote-name").value.trim();
  if (!name || name === "." || name === ".." || /[\/\\]/.test(name)) throw new Error("Enter a filename without a directory.");
  const file = $("upload-file").files[0];
  const path = $("file-route").value + encodeURIComponent(name);
  const headers = new Headers();
  if (method === "POST") {
    if (!file) throw new Error("Choose a local file to upload.");
    headers.set("Content-Type", "application/octet-stream");
  }
  return {method, path, headers, body: method === "POST" ? file : undefined,
    compareFile: method === "GET" ? file : undefined, filename: name};
}

// Compare in blocks so a full-size CGI response can be checked without making
// a second 100 MB copy of either the request or the response in JavaScript.
async function compareBytes(input, result, check, digit, signal) {
  if (input.size !== result.size) return "Size mismatch · expected " + input.size + " bytes.";
  const chunk = 256 * 1024;
  for (let offset = 0; offset < input.size; offset += chunk) {
    signal.throwIfAborted();
    const bytes = new Uint8Array(await result.slice(offset, offset + chunk).arrayBuffer());
    const source = check === "digit" ? null : new Uint8Array(await input.slice(offset, offset + chunk).arrayBuffer());
    for (let i = 0; i < bytes.length; i++) {
      let wanted = source ? source[i] : digit.charCodeAt(0);
      if (check === "upper" && wanted >= 97 && wanted <= 122) wanted -= 32;
      if (bytes[i] !== wanted) return "Body mismatch at byte " + (offset + i) + ".";
    }
  }
  return "Matched all " + result.size.toLocaleString() + " bytes.";
}

function headerLines(headers) {
  return Array.from(headers, ([key, value]) => key + ": " + value).join("\n");
}

async function previewBody(blob) {
  if (!blob.size) return "[empty body]";
  const sample = new Uint8Array(await blob.slice(0, 1024).arrayBuffer());
  let preview;
  try {
    const text = new TextDecoder("utf-8", {fatal: true}).decode(sample, {stream: blob.size > sample.length});
    if (/[\x00-\x08\x0e-\x1f]/.test(text)) throw new Error();
    preview = text;
  } catch {
    preview = Array.from(sample, byte => byte.toString(16).padStart(2, "0")).join(" ");
  }
  return preview + (blob.size > sample.length ? "\n[…] Showing the first 1,024 bytes." : "");
}

async function send(buildRequest) {
  if (activeRequest) return;
  if (downloadUrl) URL.revokeObjectURL(downloadUrl);
  $("download").hidden = true;
  $("exchange-details").hidden = true;
  $("status").className = "";
  const controller = new AbortController();
  activeRequest = controller;
  const timeout = setTimeout(() => controller.abort("timeout"), 120000);
  try {
    const {method, path, headers, body, expected, check, compareFile, filename} = buildRequest();
    $("request-info").textContent = [method + " " + path, headerLines(headers),
      body ? body.size.toLocaleString() + " request bytes" : ""].filter(Boolean).join("\n");
    $("response-info").textContent = "Awaiting response…";
    $("preview").textContent = "";
    $("exchange-details").hidden = false;
    $("status").textContent = "Sending " + method + " " + path + "…";
    $("cancel").hidden = false;
    for (const button of document.querySelectorAll("form button")) button.disabled = true;
    const start = performance.now();
    const response = await fetch(path, {method, headers, body, signal: controller.signal, mode: "same-origin", cache: "no-store"});
    const status = "HTTP " + response.status + " " + response.statusText;
    $("response-info").textContent = [status, headerLines(response.headers),
      response.redirected ? "Followed redirect to " + response.url : ""].filter(Boolean).join("\n");
    const blob = await response.blob();
    const summary = [status, blob.size.toLocaleString() + " bytes", Math.round(performance.now() - start) + " ms"];
    if (expected) summary.push(Number(expected) === response.status ? "Expected status matched" : "Expected HTTP " + expected);
    if (compareFile && response.ok || check) {
      summary.push(await compareBytes(compareFile || body || new Blob(), blob, check || "match",
        headers.get("X-Secret-Header-For-Test"), controller.signal));
    }
    $("preview").textContent = await previewBody(blob);
    if (blob.size) {
      downloadUrl = URL.createObjectURL(blob.slice(0, blob.size, "application/octet-stream"));
      $("download").href = downloadUrl;
      $("download").download = filename || "response.bin";
      $("download").hidden = false;
    }
    $("status").textContent = summary.join(" · ");
  } catch (error) {
    $("status").className = "fail";
    if (controller.signal.aborted) {
      $("status").textContent = controller.signal.reason === "timeout" ? "Request timed out after 120 seconds." : "Request canceled.";
    } else {
      $("status").textContent = error.message;
    }
    if ($("response-info").textContent === "Awaiting response…") $("response-info").textContent = "No response headers received.";
  } finally {
    clearTimeout(timeout);
    activeRequest = null;
    $("cancel").hidden = true;
    for (const button of document.querySelectorAll("form button")) button.disabled = false;
  }
}

$("listener").selectedIndex = secondary ? 2 : intra ? 1 : 0;
$("listener").onchange = () => location.assign($("listener").value);
$("upload-form").hidden = intra || secondary;
$("file-listener-link").hidden = !intra && !secondary;
if ($("upload-form").hidden) $("files-note").textContent = "The upload and read-only routes are configured on the Quick tests listener.";

if (secondary) {
  addPresets("Listener B", [
    {title: "Read the second website", method: "GET", path: "/", draft: {}},
    {title: "Request a fallback error page", method: "GET", path: "/missing", draft: {status: 404}}
  ]);
} else {
  for (const group of intra ? officialCollections() : defenseCollections) addPresets(group.name, group.cases);
  if (!intra) addPresets("Cookies", [{title: "Read and clear the cookie", method: "GET", path: "/cgi/cookie.py", draft: {}}]);
}
const requestedPreset = new URLSearchParams(location.search).get("case");
if (presets.has(requestedPreset)) $("preset").value = requestedPreset;
loadPreset();
$("preset").onchange = loadPreset;
$("method").oninput = updateBodyFields;
$("body-kind").onchange = updateBodyFields;
$("request-form").onsubmit = event => {
  event.preventDefault();
  send(composeRequest);
};
$("upload-file").onchange = () => {
  const file = $("upload-file").files[0];
  if (file) $("remote-name").value = file.name;
};
$("upload-form").onsubmit = event => {
  event.preventDefault();
  send(() => fileRequest(event.submitter?.value || "POST"));
};
$("cancel").onclick = () => activeRequest?.abort();
