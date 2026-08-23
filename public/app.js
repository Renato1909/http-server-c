(function () {
  var dot = document.getElementById("status-dot");
  var text = document.getElementById("health-text");

  fetch("/health")
    .then(function (res) {
      if (!res.ok) throw new Error("status " + res.status);
      return res.text();
    })
    .then(function (body) {
      if (body.trim() === "ok") {
        dot.className = "up";
        text.textContent = "/health respondeu ok";
      } else {
        throw new Error("corpo inesperado: " + body);
      }
    })
    .catch(function () {
      dot.className = "down";
      text.textContent = "falha ao contatar /health";
    });
})();
