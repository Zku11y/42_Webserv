"use strict";

const copyStatus = document.getElementById("copy-status");
const buttons = document.querySelectorAll("[data-copy]");
const sidebarLinks = document.querySelectorAll(".sidebar a[href^='#']");

function selectSnippet(snippet) {
  const selection = window.getSelection();
  const range = document.createRange();
  range.selectNodeContents(snippet);
  selection.removeAllRanges();
  selection.addRange(range);
}

buttons.forEach((button) => {
  button.hidden = false;
  button.addEventListener("click", async () => {
    const snippet = button.closest("figure").querySelector("code");

    try {
      await navigator.clipboard.writeText(snippet.textContent);
      button.textContent = "Copied";
      copyStatus.textContent = "Code copied to the clipboard.";
    } catch {
      selectSnippet(snippet);
      button.textContent = "Selected";
      copyStatus.textContent = "Code selected. Use your device's copy command.";
    }

    window.setTimeout(() => { button.textContent = "Copy code"; }, 1800);
  });
});

function updateCurrentLink() {
  const hash = window.location.hash;
  sidebarLinks.forEach((link) => {
    if (link.hash === hash) {
      link.setAttribute("aria-current", "location");
    } else {
      link.removeAttribute("aria-current");
    }
  });
}

updateCurrentLink();
window.addEventListener("hashchange", updateCurrentLink);
