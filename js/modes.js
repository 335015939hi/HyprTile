// ============================================================
// MODES.JS – zentrale Steuerung aller UI-Modi
// ============================================================

// Standardzustand
window.activeMode = "grid";

// Container für Keyboard-Mode-Handler
window.modeHandlers = window.modeHandlers || {};

window.closeAllPanelsExcept = function(keepMode) {

    const closeIfNot = (mode, closeFn) => {
        if (keepMode !== mode) closeFn();
    };

    closeIfNot("sysmon", () => {
        document.getElementById("sysmon").classList.remove("visible");
    });

    closeIfNot("notes", () => {
        document.getElementById("notesPanel").classList.remove("visible");
    });

    closeIfNot("capture", () => {
        document.getElementById("capturePanel").classList.add("hidden");
    });

    closeIfNot("monitor", () => {
        document.getElementById("monitor-grid").classList.add("hidden");
    });

    closeIfNot("calendar", () => {
        document.getElementById("calendar").classList.remove("visible");
    });

    closeIfNot("addApp", () => {
        document.getElementById("addAppDialog").classList.add("hidden");
    });

    closeIfNot("iconChooser", () => {
        document.getElementById("bigIconChooser").classList.add("hidden");
    });

    // Grid sichtbar machen wenn kein anderer Modus aktiv ist
    if (keepMode === "grid" || keepMode === null) {
        document.getElementById("appgrid").style.display = "grid";
    } else {
        document.getElementById("appgrid").style.display = "none";
    }
};

// ------------------------------------------------------------
// Mode-Debugger (sichtbar oben rechts, extrem hoher z-index)
// ------------------------------------------------------------
(function initModeDebugger() {

    const dbg = document.getElementById("modeDebugger");
    if (!dbg) return;

    Object.assign(dbg.style, {
        position: "fixed",
        top: "10px",
        right: "10px",
        padding: "6px 12px",
        background: "rgba(0,0,0,0.55)",
        color: "#00f7ff",
        fontFamily: "monospace",
        fontSize: "14px",
        borderRadius: "6px",
        border: "1px solid rgba(0,255,255,0.6)",
        backdropFilter: "blur(4px)",
        zIndex: "99999999999999999",
        pointerEvents: "none"
    });

    dbg.textContent = "MODE: GRID";
})();

function updateModeDebugger(m) {
    const dbg = document.getElementById("modeDebugger");
    if (!dbg) return;
    dbg.textContent = "MODE: " + m.toUpperCase();
}


// ------------------------------------------------------------
// ZENTRALE FUNKTION: Modus setzen
// ------------------------------------------------------------
window.setMode = function(mode) {

    console.log(`[MODE] ${window.activeMode} → ${mode}`);

    // Erst ALLE anderen Modi schließen
    window.closeAllPanelsExcept(mode);

    // Dann Modus setzen
    window.activeMode = mode;
    
    //DEBUG
	updateModeDebugger(mode);

    // Event auslösen falls jemand lauscht
    document.dispatchEvent(new CustomEvent("modechange", { detail: mode }));
};


// ------------------------------------------------------------
// Alias für alten Code
// ------------------------------------------------------------
window.setActiveMode = function(mode) {
    window.setMode(mode);
};


// ------------------------------------------------------------
// Panel-basierter Moduswechsel (optional genutzt)
// ------------------------------------------------------------
window.setModeForPanel = function(panelElementId, modeIfVisible, modeIfHidden = "grid") {

    const el = document.getElementById(panelElementId);
    if (!el) return;

    const visible =
        !el.classList.contains("hidden") &&
        !el.classList.contains("invisible") &&
        !el.classList.contains("collapsed") &&
        !el.classList.contains("not-visible");

    window.setMode(visible ? modeIfVisible : modeIfHidden);
};


// ------------------------------------------------------------
// Utility-Funktion
// ------------------------------------------------------------
window.getCurrentMode = function() {
    return window.activeMode;
};
