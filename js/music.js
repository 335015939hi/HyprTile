//
// music.js — angepasst an das neue globale Mode-System
//

// Globale States
let musicCursor = 0;
let musicResults = [];
let backendRef = null;

// Fokus-Status
let musicFocus = "input";   // "input" | "list"

function highlightMusicList() {
    document.getElementById("musicResults").classList.add("music-list-focused");
}

function unhighlightMusicList() {
    document.getElementById("musicResults").classList.remove("music-list-focused");
}



// =====================================================
// INITIALISIERUNG
// =====================================================
function initMusic(backend) {

    backendRef = backend;

    const inp = document.getElementById("musicInput");


    // ---------------------------------------------------------
    // INPUT KEYDOWN  (nur gültig wenn Mode = "music")
    // ---------------------------------------------------------
    inp.addEventListener("keydown", e => {

        if (window.activeMode !== "music") return;
        if (musicFocus !== "input") return;

        // ENTER → Song starten (auch wenn Fokus im Input ist)
		if (e.key === "Enter") {
			e.preventDefault();
			e.stopImmediatePropagation();

			const item = musicResults[musicCursor];
			if (item) {
				backendRef.playMusic(item.full);
				closeMusicMode();
			}
			return;
		}

		// Pfeiltasten → wenn im Input gedrückt → automatisch in die Liste wechseln
		if (e.key === "ArrowDown" || e.key === "ArrowUp") {
			e.preventDefault();
			e.stopImmediatePropagation();

			// Wenn wir gerade im Input sind → automatisch in LIST wechseln
			// (genau wie TAB, nur automatisch)
			musicFocus = "list";
			highlightMusicList();

			const results = document.getElementById("musicResults");
			results.focus();

			// Danach Cursor normal bewegen
			if (e.key === "ArrowUp" && musicCursor > 0)
				musicCursor--;
			else if (e.key === "ArrowDown" && musicCursor < musicResults.length - 1)
				musicCursor++;

			updateMusicSelection();
			return;
		}


        // ESC → schließen
        if (e.key === "Escape") {
            e.preventDefault();
            e.stopImmediatePropagation();
            closeMusicMode();
            return;
        }

    }, true);



    // ---------------------------------------------------------
    // LIVE-SUCHE
    // ---------------------------------------------------------
    inp.addEventListener("input", () => {
        if (window.activeMode !== "music") return;

        backend.filterMusic(inp.value).then(arr => {
            musicResults = arr;
            musicCursor = 0;
            renderMusicResults();
        });
    });



    // ==========================================================
    // GLOBALE TAB-LOGIK (nur im Music Mode)
    // ==========================================================
    document.addEventListener("keydown", e => {

        if (window.activeMode !== "music") return;

        if (e.key === "Tab") {
            e.preventDefault();
            e.stopImmediatePropagation();

            if (musicFocus === "input") {

                if (musicResults.length > 0) {

                    musicFocus = "list";

                    if (musicCursor < 0 || musicCursor >= musicResults.length)
                        musicCursor = 0;

                    updateMusicSelection();

                    inp.blur();
                    document.getElementById("musicResults").focus();
                    highlightMusicList();
                }
                return;
            }

            if (musicFocus === "list") {
                musicFocus = "input";
                inp.focus();
                unhighlightMusicList();
                return;
            }
        }

    }, true);



    // ---------------------------------------------------------
    // LISTEN-STEUERUNG (nur im Music Mode)
    // ---------------------------------------------------------
    document.addEventListener("keydown", e => {

        if (window.activeMode !== "music") return;

        e.stopImmediatePropagation();

        if (musicFocus === "list") {

            // ESC → zurück ins Input
            if (e.key === "Escape") {
                e.preventDefault();
                musicFocus = "input";
                inp.focus();
                unhighlightMusicList();
                return;
            }

            // ENTER → spielen
            if (e.key === "Enter") {
                e.preventDefault();
                const item = musicResults[musicCursor];
                if (item) {
                    backendRef.playMusic(item.full);
                    closeMusicMode();
                }
                return;
            }

            // Pfeile
            if (e.key === "ArrowDown") {
                if (musicCursor < musicResults.length - 1)
                    musicCursor++;
                updateMusicSelection();
                e.preventDefault();
                return;
            }

            if (e.key === "ArrowUp") {
                if (musicCursor > 0)
                    musicCursor--;
                updateMusicSelection();
                e.preventDefault();
                return;
            }

            return;
        }

        // ESC → schließen (nur im Input)
        if (e.key === "Escape") {
            e.preventDefault();
            closeMusicMode();
            return;
        }

    }, true);



    // ---------------------------------------------------------
    // SHORTCUT "m " → nur im GRID-MODE sinnvoll
    // ---------------------------------------------------------
    document.addEventListener("keydown", e => {

        if (window.activeMode !== "grid") return;

        if (e.key === " ") {
            if (window.filterBuffer === "m") {
                e.preventDefault();
                e.stopImmediatePropagation();
                openMusicMode();
                return;
            }
        }
    }, true);



    // ---------------------------------------------------------
    // Maus-Hover Fokus
    // ---------------------------------------------------------
    const results = document.getElementById("musicResults");

    results.addEventListener("mouseenter", () => {
        highlightMusicList();
    });

    results.addEventListener("mouseleave", () => {
        if (musicFocus !== "list") {
            unhighlightMusicList();
        }
    });

} // initMusic







// =====================================================
// MUSIC MODE ÖFFNEN
// =====================================================
function openMusicMode() {

    setActiveMode("music");

    musicFocus = "input";
    musicResults = [];
    musicCursor = 0;

    document.getElementById("appgrid").style.display = "none";
    document.getElementById("musicPanel").classList.remove("hidden");

    window.filterBuffer = "";
    if (typeof applyFilter === "function") applyFilter();

    const inp = document.getElementById("musicInput");
    inp.value = "";
    inp.focus();
    setTimeout(() => inp.focus(), 30);

    renderMusicResults();
}



// =====================================================
// MUSIC MODE SCHLIESSEN
// =====================================================
function closeMusicMode() {
	
    setActiveMode("grid");

    musicFocus = "input";

    document.getElementById("musicPanel").classList.add("hidden");
    document.getElementById("appgrid").style.display = "grid";

    /*window.filterBuffer = "";
    if (typeof applyFilter === "function") applyFilter();
*/
	if (typeof filterBuffer !== "undefined") {
        filterBuffer = "";
        applyFilter();
    }

    if (typeof resetSelectionToFirstVisible === "function") {
        resetSelectionToFirstVisible();
    }
}



// =====================================================
// LISTE RENDERN
// =====================================================
function renderMusicResults() {
    const out = document.getElementById("musicResults");
    out.innerHTML = "";

    musicResults.forEach((item, index) => {

        const div = document.createElement("div");
        div.className = "music-item";
        div.dataset.index = index;
        div.textContent = item.short;

        // Click = Play
        div.addEventListener("click", () => {
			backendRef.playMusic(item.full);
			closeMusicMode();
		});

        out.appendChild(div);
    });

    updateMusicSelection();
}



// =====================================================
// AUSWAHL UPDATEN
// =====================================================
function updateMusicSelection() {
    const items = document.querySelectorAll(".music-item");

    items.forEach((el, idx) => {
        el.classList.toggle("sel", idx === musicCursor);
    });

    if (musicFocus === "list") {
        highlightMusicList();
    } else {
        unhighlightMusicList();
    }
}



// Export
window.initMusic = initMusic;
window.openMusicMode = openMusicMode;
window.closeMusicMode = closeMusicMode;
