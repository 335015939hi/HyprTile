function initSysmon() {

    // Öffnen/Schließen per Event (F1)
    document.addEventListener("toggleSysmon", () => toggleSystemMonitor());

    // Backend → Systeminfo aktualisieren
    window.updateSysInfo = function(cpu, ramUsed, ramTotal, cpuTemp) {
        const cpuPct = Math.round(cpu);
        const ramPct = Math.round(ramUsed / ramTotal * 100);

        document.getElementById("cpuBar").style.width = cpuPct + "%";
        document.getElementById("cpuVal").textContent = cpuPct + "%";

        document.getElementById("ramBar").style.width = ramPct + "%";
        document.getElementById("ramVal").textContent = ramPct + "%";

        if (cpuTemp >= 0) {
            document.getElementById("tempBar").style.width = 
                Math.min(cpuTemp, 100) + "%";
            document.getElementById("tempVal").textContent =
                Math.round(cpuTemp) + "°C";
        }
    };
}



function toggleSystemMonitor() {
    const panel = document.getElementById("sysmon");
    const grid  = document.getElementById("appgrid");

    const show = panel.classList.toggle("visible");

    // ⭐ Modus setzen (NEU)
    window.setMode(show ? "sysmon" : "grid");

    if (show) {
        // Filter löschen, damit keine gefilterten Kacheln aktiv bleiben
        if (typeof filterBuffer !== "undefined") {
            filterBuffer = "";
            applyFilter();
        }

        grid.style.display = "none";

    } else {
        grid.style.display = "grid";
    }
}



// Globale Sichtbarkeit herstellen
window.initSysmon = initSysmon;
window.toggleSystemMonitor = toggleSystemMonitor;
