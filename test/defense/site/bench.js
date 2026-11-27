"use strict";

const official = location.pathname.startsWith("/bench/");
const collections = official ? officialCollections() : defenseCollections;
const mandatory = collections.filter(c => !c.bonus).flatMap(c => c.cases);
const batch = collections.filter(c => !c.bonus && !c.individual).flatMap(c => c.cases);
const batchLabel = official ? "Run " + batch.length + " quick cases" : "Run mandatory checks";
let selected = collections[0];
let busy = false;

function element(tag, className, value) {
  const node = document.createElement(tag);
  node.className = className;
  node.textContent = value;
  return node;
}

function showCollection(group) {
  selected = group;
  $("collection-title").textContent = group.name;
  $("category").textContent = official ? "Supplied tester" : group.bonus ? "Optional" : "Mandatory";
  $("collection-count").textContent = group.cases.length + " cases";
  $("collection-note").textContent = group.note;
  $("file-option").hidden = group.name !== "File operations";
  for (const [i, button] of Array.from($("collections").children).entries()) {
    const failures = collections[i].cases.filter(item => item.state === "Failed").length;
    button.lastChild.textContent = failures ? failures + " failed" : collections[i].cases.length;
    button.lastChild.classList.toggle("fail", failures > 0);
    if (collections[i] === group) button.setAttribute("aria-current", "true");
    else button.removeAttribute("aria-current");
  }
  $("case-list").replaceChildren();
  for (const item of group.cases) {
    const row = element("section", "case", "");
    const top = document.createElement("div");
    const heading = element("div", "case-head", "");
    const meta = element("div", "meta", "");
    meta.append(element("span", "method", item.method), element("code", "", item.path));
    heading.append(element("h3", "", item.title), meta);
    const state = element("span", "badge", item.state);
    state.dataset.state = item.state;
    const button = element("button", "run", "Run");
    button.setAttribute("aria-label", "Run " + item.title);
    button.disabled = busy;
    button.onclick = () => runCases([item]);
    const edit = element("a", "edit-request", "Edit request");
    edit.href = item.draft.files ? "manual.html#files" : "manual.html?case=" + encodeURIComponent(item.title);
    edit.setAttribute("aria-label", "Edit request for " + item.title);
    top.append(heading, state, edit, button);
    const details = document.createElement("details");
    details.open = item.open || false;
    details.ontoggle = () => { if (details.isConnected) item.open = details.open; };
    details.append(element("summary", "", "Expected result & exchange"), element("p", "", item.expected));
    if (item.log.length) details.append(element("pre", "", item.log.join("\n")));
    row.append(top, details);
    $("case-list").append(row);
  }
}

async function runCases(items) {
  if (busy) return;
  busy = true;
  $("run-suite").disabled = true;
  $("payload").disabled = true;
  for (const [index, item] of items.entries()) {
    if (items === batch) $("suite-label").textContent = "Running " + (index + 1) + " of " + items.length;
    item.log = [];
    item.state = "Running";
    showCollection(selected);
    try {
      await item.run(item.log);
      item.state = "Passed";
    } catch (error) {
      item.state = "Failed";
      item.open = true;
      item.log.push("FAIL · " + error.message);
    }
    $("passed").textContent = mandatory.filter(c => c.state === "Passed").length;
    $("failed").textContent = mandatory.filter(c => c.state === "Failed").length;
  }
  busy = false;
  $("suite-label").textContent = batchLabel;
  $("run-suite").disabled = false;
  $("payload").disabled = false;
  showCollection(selected);
  $("announcement").textContent = "Finished. " + items.filter(i => i.state === "Passed").length + " of " + items.length + " passed.";
}

for (const group of collections) {
  const button = element("button", "collection", group.name);
  button.append(element("span", "count", String(group.cases.length)));
  button.onclick = () => showCollection(group);
  $("collections").append(button);
}
$("target").textContent = location.host;
$("total").textContent = mandatory.length;
if (official) {
  document.title = "Intra tester · Webserv";
  document.querySelector("h1").firstChild.textContent = "Intra tester";
  document.querySelector(".metrics").setAttribute("aria-label", "Intra browser results");
  $("defense-link").removeAttribute("aria-current");
  $("official-link").setAttribute("aria-current", "page");
  $("intro-note").textContent = "Replay individual cases against the YoupiBanane profile. Use the supplied binary below for its exact wire requests and worker stages.";
  $("native").hidden = false;
}
$("suite-label").textContent = batchLabel;
$("run-suite").onclick = () => runCases(batch);
$("run-suite").disabled = false;
showCollection(selected);
