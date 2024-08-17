function showLoading() {
  document.getElementById("loading").style.display = "block";
}

function hideLoading() {
  document.getElementById("loading").style.display = "none";
}

function showAlert(message) {
  document.getElementById("alertModalBody").textContent = message;
  new bootstrap.Modal(document.getElementById("alertModal")).show();
}

function handleResponseCode(code) {
  switch (code) {
    case 1:
      showAlert("Operazione di apertura completata con successo");
      break;
    case 2:
      showAlert("Operazione di stop completata con successo");
      break;
    case 3:
      showAlert("Cancello in movimento");
      break;
    case 4:
      showAlert("Operazione di apertura fissa in esecuzione");
      break;
    case 5:
      showAlert("Errore di deserializzazione durante l'impostazione");
      break;
    case 6:
      showAlert("Impostazioni salvate");
      break;
    case 7:
      showAlert("Nessun dato inviato per l'impostazione");
      break;
    case 8:
      showAlert("Impostazioni restituite con successo");
      break;
    case 9:
      showAlert("Stato del cancello restituito con successo");
      break;
    case 10:
      showAlert("Operazione di apertura fissa eseguita con successo");
      break;
    default:
      showAlert("Operazione non riuscita");
      break;
  }
}

function sendCommand(cmd) {
  showLoading();
  fetch("/" + cmd)
    .then((response) => {
      if (!response.ok) {
        throw new Error("Errore di rete");
      }
      return response.json();
    })
    .then((data) => {
      handleResponseCode(data.code);
      if (cmd === "stop" && data.code === 2) {
        toggleStopButton(false);
      } else if (cmd === "open" && data.code === 1) {
        toggleStopButton(true);
      }
      updateStatus();

      document.getElementById("connection-status-false").style.display = "none";
    })
    .catch((error) => {
      document.getElementById("connection-status-false").style.display =
        "block";
    })
    .finally(hideLoading);
}

function toggleStopButton(isStop) {
  const stopBtn = document.getElementById("stopBtn");
  if (isStop) {
    stopBtn.textContent = "Stop";
    stopBtn.onclick = function () {
      sendCommand("stop");
    };
  } else {
    stopBtn.textContent = "Riprendi";
    stopBtn.onclick = function () {
      sendCommand("open");
    };
  }
}

function updateSettings() {
  showLoading();
  const settings = {
    enableL: document.getElementById("enableL").checked,
    lightOnTime: timeToMinutes(document.getElementById("lightOnTime").value),
    lightOffTime: timeToMinutes(document.getElementById("lightOffTime").value),
    lightDuration: document.getElementById("lightDuration").value,
    password: document.getElementById("password").value,
    behavior: document.getElementById("behavior").value,
  };
  fetch("/setsettings", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(settings),
  })
    .then((response) => response.json())
    .then((data) => {
      handleResponseCode(data.code);
    })
    .finally(hideLoading);
}

function timeToMinutes(time) {
  const [hours, minutes] = time.split(":");
  return parseInt(hours) * 60 + parseInt(minutes);
}

function minutesToTime(minutes) {
  const hours = Math.floor(minutes / 60);
  const mins = minutes % 60;
  return `${hours.toString().padStart(2, "0")}:${mins
    .toString()
    .padStart(2, "0")}`;
}

function updateStatus() {
  fetch("/getstatus")
    .then((response) => {
      if (!response.ok) {
        throw new Error("Errore di rete");
      }
      return response.json();
    })
    .then((data) => {
      if (data.code === 9) {
        let gateStatusText;
        let gateStatusCode = data.gateStatus;

        if (gateStatusCode === 1) {
          gateStatusText = "Aperto";
        } else if (gateStatusCode === 2) {
          gateStatusText = "Chiuso";
        } else if (gateStatusCode === 3) {
          gateStatusText = "In movimento";
        }
        document.getElementById("status").textContent =
          "Stato cancello: " + gateStatusText;

        let lightStatusText = data.lightStatus === 1 ? "Accesa" : "Spenta";
        document.getElementById("light-status").textContent =
          "Stato luce: " + lightStatusText;

        if (gateStatusCode === 3) {
          document.getElementById("openBtn").disabled = true;
          document.getElementById("openBtn").innerHTML = "-";
        } else if (gateStatusCode === 1) {
          document.getElementById("openBtn").disabled = false;
          document.getElementById("openBtn").innerHTML = "Chiudi";
        } else {
          document.getElementById("openBtn").disabled = false;
          document.getElementById("openBtn").innerHTML = "Apri";
        }
      } else {
        handleResponseCode(data.code);
      }

      document.getElementById("connection-status-false").style.display = "none";
    })
    .catch((error) => {
      document.getElementById("connection-status-false").style.display =
        "block";
    });
}

fetch("/getsettings")
  .then((response) => response.json())
  .then((data) => {
    if (data.code === 8) {
      document.getElementById("enableL").checked = data.settings.enableL;
      document.getElementById("lightOnTime").value = minutesToTime(
        data.settings.lightOnTime
      );
      document.getElementById("lightOffTime").value = minutesToTime(
        data.settings.lightOffTime
      );
      document.getElementById("password").value = data.settings.password;
      document.getElementById("lightDuration").value =
        data.settings.lightDuration;
      document.getElementById("behavior").value = data.settings.behavior;
      handleResponseCode(data.code);
    } else {
      handleResponseCode(data.code);
    }
  });

updateStatus();

setInterval(updateStatus, 1000); // Aggiorna lo stato ogni secondo

const darkModeToggle = document.getElementById("darkModeToggle");
const body = document.body;

// Carica lo stato del tema scuro dai cookie
if (localStorage.getItem("darkMode") === "enabled") {
  body.classList.add("dark-mode");
  darkModeToggle.checked = true;
}

// Attiva/disattiva il tema scuro al clic
darkModeToggle.addEventListener("click", () => {
  if (body.classList.contains("dark-mode")) {
    body.classList.remove("dark-mode");
    localStorage.setItem("darkMode", "disabled");
  } else {
    body.classList.add("dark-mode");
    localStorage.setItem("darkMode", "enabled");
  }
});

function setThemeBasedOnDevicePreference() {
  const darkModeToggle = document.getElementById("darkModeToggle");
  const prefersDarkScheme = window.matchMedia("(prefers-color-scheme: dark)");

  function applyTheme(isDark) {
    document.body.classList.toggle('dark-mode', isDark);
    darkModeToggle.checked = isDark;
    localStorage.setItem('darkMode', isDark ? 'enabled' : 'disabled');
  }

  const storedTheme = localStorage.getItem("darkMode");
  if (storedTheme === "enabled") {
    applyTheme(true);
  } else if (storedTheme === "disabled") {
    applyTheme(false);
  } else {
    applyTheme(prefersDarkScheme.matches);
  }

  prefersDarkScheme.addEventListener('change', (e) => {
    if (localStorage.getItem("darkMode") === null) {
      applyTheme(e.matches);
    }
  });

  darkModeToggle.addEventListener("change", () => {
    applyTheme(darkModeToggle.checked);
  });
}

document.addEventListener("DOMContentLoaded", setThemeBasedOnDevicePreference);