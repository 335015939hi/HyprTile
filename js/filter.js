// globale Filtervariable
let filterBuffer = "";

function initFilter() {

    document.addEventListener("keydown", e => {

        // =====================================================
        // 0) Filter darf NUR im grid-Modus aktiv sein
        // =====================================================
        if (window.activeMode !== "grid") {
            return;
        }

        // =====================================================
        // 1) Shortcut: "m " → Music Mode starten
        // =====================================================
        if (e.key === " " && filterBuffer === "m") {
            e.preventDefault();
            e.stopImmediatePropagation();

            if (typeof openMusicMode === "function") {
                openMusicMode();
            }
            return;
        }
        
        // "v " → Video Mode starten
		if (e.key === " " && filterBuffer === "v") {
			e.preventDefault();
			e.stopImmediatePropagation();
			if (typeof openVideoMode === "function") openVideoMode();
			return;
		}


        // =====================================================
        // 2) ESC → Filter löschen (nicht Launcher schließen)
        // =====================================================
        if (e.key === "Escape") {

            if (filterBuffer.length > 0) {
                filterBuffer = "";
                applyFilter();

                e.preventDefault();
                e.stopPropagation();
                return;
            }

            // Wenn Filter schon leer ist → weiterreichen an keyboard.js
            return;
        }

        // =====================================================
        // 3) Backspace → letztes Zeichen löschen
        // =====================================================
        if (e.key === "Backspace") {

            if (filterBuffer.length > 0) {
                filterBuffer = filterBuffer.slice(0, -1);
                applyFilter();
            }

            e.preventDefault();
            return;
        }

        // =====================================================
        // 4) Normale Zeichen → Filter füttern
        // =====================================================
        if (!e.ctrlKey && !e.metaKey && !e.altKey && e.key.length === 1) {

            filterBuffer += e.key.toLowerCase();
            applyFilter();
            return;
        }

        // Alles andere ignorieren
    });
}



// =====================================================
// FILTER-ANWENDUNG
// =====================================================
function applyFilter() {

    // Sicherheitscheck → nur im Grid-Modus
    if (window.activeMode !== "grid") return;

    const items = document.querySelectorAll(".app");
    const term = filterBuffer.toLowerCase();

    items.forEach(el => {
        const id = el.dataset.id.toLowerCase();
        const name = el.querySelector(".inner div").textContent.toLowerCase();

        const match =
            term === "" ||
            id.includes(term) ||
            name.includes(term);

        el.style.display = match ? "flex" : "none";
    });

    resetSelectionToFirstVisible();
}



// =====================================================
// AUTOMATISCHE SELEKTION
// =====================================================
function resetSelectionToFirstVisible() {

    if (window.activeMode !== "grid") return;

    const visible = Array.from(document.querySelectorAll(".app"))
        .filter(el => el.style.display !== "none");

    if (!visible.length) return;

    const all = Array.from(document.querySelectorAll(".app"));
    const firstIndex = all.indexOf(visible[0]);

    if (window.setSelectedIndex) {
        window.setSelectedIndex(firstIndex);
    }
}


// Export
window.initFilter = initFilter;
