// re2DJ project site (Task 432).
// Remembers the language the reader picked with the KO/EN switch. The first-visit
// redirect itself is inline in <head> so the page never flashes the wrong language.
(function () {
  "use strict";

  var KEY = "re2dj-lang";

  document.addEventListener("click", function (event) {
    var link = event.target.closest && event.target.closest("[data-lang-switch]");
    if (!link) {
      return;
    }
    try {
      window.localStorage.setItem(KEY, link.getAttribute("data-lang-switch"));
    } catch (error) {
      // Storage can be blocked; the switch still navigates.
    }
  });
})();
