"use strict";

// The native tester accepts arbitrary static file contents. The browser also
// checks the contents of this checkout's fixtures to catch incorrect routing.
function officialGet(title, path, contents, status = 200) {
  const expected = contents === undefined ? "Expect HTTP " + status + "." :
    "Expect HTTP " + status + " and the contents of the mapped fixture.";
  return test(title, "GET", path, expected, async log => {
    const result = await exchange(log, "GET", path, status);
    if (contents !== undefined) requireResult(result.text === contents, "Unexpected fixture contents.");
  }, {status});
}

function officialPost(size, status) {
  return test("POST " + size + " bytes", "POST", "/post_body",
    "Expect HTTP " + status + " with a 100-byte route limit.", log =>
      exchange(log, "POST", "/post_body", status, new Uint8Array(size), {headers: {"Content-Type": "test/file"}}),
    {size, status, type: "test/file"});
}

function repeatedBody(size) {
  const block = new Blob(["a".repeat(65536)]);
  const parts = Array(Math.floor(size / block.size)).fill(block);
  parts.push(block.slice(0, size % block.size));
  return new Blob(parts);
}

function officialCgi(title, path, size, useHeader = false) {
  const digit = "7";
  const output = useHeader ? digit : "A";
  return test(title, "POST", path,
    "Send " + size.toLocaleString("en-US") + " lowercase bytes. Expect 200 and the same number of " +
    (useHeader ? "copies of the digit in X-Secret-Header-For-Test." : "uppercase bytes."),
    async log => {
      const headers = {"Content-Type": "test/file"};
      if (useHeader) headers["X-Secret-Header-For-Test"] = digit;
      await exchange(log, "POST", path, 200, repeatedBody(size), {
        headers, timeout: 120000,
        async checkBody(response, evidence) {
          requireResult(response.body, "CGI returned no body.");
          const reader = response.body.getReader();
          const expectedByte = output.charCodeAt(0);
          let count = 0;
          while (true) {
            const {value, done} = await reader.read();
            if (done) break;
            const mismatch = value.findIndex(byte => byte !== expectedByte);
            requireResult(mismatch === -1, "Unexpected output at byte " + (count + mismatch) + ".");
            count += value.length;
          }
          evidence.push("Received " + count + " bytes.");
          requireResult(count === size, "Expected " + size + " output bytes.");
        }
      });
    }, {size, type: "test/file", headers: useHeader ? "X-Secret-Header-For-Test: " + digit : "",
      check: useHeader ? "digit" : "upper"});
}

function officialCollections() {
  return [
    {name: "Root methods", note: "The tester config permits GET on the root route.", cases: [
      officialGet("Read the default site", "/", "This is the default index yo!\n"),
      test("Reject POST at the root", "POST", "/", "Expect 405 for an empty POST.", log =>
        exchange(log, "POST", "/", 405, ""), {status: 405}),
      test("Reject HEAD at the root", "HEAD", "/", "Expect 405.", log =>
        exchange(log, "HEAD", "/", 405), {status: 405})
    ]},
    {name: "Directory mapping", note: "Follow redirects and check the index and files served under /directory.", cases: [
      officialGet("Resolve the directory index", "/directory", "Nope"),
      officialGet("Read the index by name", "/directory/youpi.bad_extension", "Nope"),
      officialGet("GET the CGI fixture", "/directory/youpi.bla", ""),
      officialGet("Request a missing entry", "/directory/oulalala", undefined, 404),
      officialGet("Resolve the nested index", "/directory/nop", "bad extension?\n"),
      officialGet("Read the nested index with a slash", "/directory/nop/", "bad extension?\n"),
      officialGet("Read a nested file", "/directory/nop/other.pouic", "pouic content\n"),
      officialGet("Request the misspelled nested file", "/directory/nop/other.pouac", undefined, 404),
      officialGet("Open a directory with no index", "/directory/Yeah", undefined, 404),
      officialGet("Read a file in the unlisted directory", "/directory/Yeah/not_happy.bad_extension", "unhappyyyyy\n")
    ]},
    {name: "POST limit", note: "The binary sends bodies of 0, 100, 200 and 101 bytes. A successful POST may return any body.", cases: [
      officialPost(0, 200), officialPost(100, 200), officialPost(200, 413), officialPost(101, 413)
    ]},
    {name: "CGI header", note: "The official CGI helper receives the request header through its environment.", cases: [
      officialCgi("Pass a custom header to CGI", "/directory/youpi.bla", 100000, true)
    ]},
    {name: "Large CGI", individual: true, note: "Run each 100 MB exchange individually. The browser verifies every output byte. Use the native tester for chunked request framing.", cases: [
      officialCgi("POST to the existing CGI path", "/directory/youpi.bla", 100000000),
      officialCgi("POST to the missing .bla path", "/directory/youpla.bla", 100000000)
    ]}
  ];
}
