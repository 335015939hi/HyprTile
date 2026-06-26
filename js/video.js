//
// video.js — identisch aufgebaut wie music.js, aber für Videos
//

// Globale States
let videoCursor = 0;
let videoResults = [];
let videoBackendRef = null;

// Fokus-Status
let videoFocus = "input";   // "input" | "list"

function highlightVideoList() {
    document.getElementById("videoResults").classList.add("video-list-focused");
}

function unhighlightVideoList() {
    document.getElementById("videoResults").classList.remove("video-list-focused");
}



// =====================================================
// INITIALISIERUNG
// =====================================================
function initVideo(backend) {

    videoBackendRef = backend;

    const inp = document.getElementById("videoInput");


    // ---------------------------------------------------------
    // INPUT KEYDOWN  (nur gültig wenn Mode = "video")
    // ---------------------------------------------------------
    inp.addEventListener("keydown", e => {

        if (window.activeMode !== "video") return;
        if (videoFocus !== "input") return;

        // ENTER → Video starten (auch im Input)
        if (e.key === "Enter") {
            e.preventDefault();
            e.stopImmediatePropagation();

            const item = videoResults[videoCursor];
            if (item) {
                videoBackendRef.playVideo(item.full);
                closeVideoMode();
                backend.toggleFromJS();
            }
            return;
        }

        // Pfeiltasten → Wechsel in Liste
        if (e.key === "ArrowDown" || e.key === "ArrowUp") {
            e.preventDefault();
            e.stopImmediatePropagation();

            videoFocus = "list";
            highlightVideoList();

            const results = document.getElementById("videoResults");
            results.focus();

            if (e.key === "ArrowUp" && videoCursor > 0)
                videoCursor--;
            else if (e.key === "ArrowDown" && videoCursor < videoResults.length - 1)
                videoCursor++;

            updateVideoSelection();
            return;
        }

        // ESC → schließen
        if (e.key === "Escape") {
            e.preventDefault();
            e.stopImmediatePropagation();
            closeVideoMode();
            return;
        }

    }, true);



    // ---------------------------------------------------------
    // LIVE-SUCHE
    // ---------------------------------------------------------
    inp.addEventListener("input", () => {
        if (window.activeMode !== "video") return;

        backend.filterVideos(inp.value).then(arr => {
            videoResults = arr;
            videoCursor = 0;
            renderVideoResults();
        });
    });



    // ==========================================================
    // GLOBALE TAB-LOGIK (nur im Video Mode)
    // ==========================================================
    document.addEventListener("keydown", e => {

        if (window.activeMode !== "video") return;

        if (e.key === "Tab") {
            e.preventDefault();
            e.stopImmediatePropagation();

            if (videoFocus === "input") {

                if (videoResults.length > 0) {

                    videoFocus = "list";

                    if (videoCursor < 0 || videoCursor >= videoResults.length)
                        videoCursor = 0;

                    updateVideoSelection();

                    inp.blur();
                    document.getElementById("videoResults").focus();
                    highlightVideoList();
                }
                return;
            }

            if (videoFocus === "list") {
                videoFocus = "input";
                inp.focus();
                unhighlightVideoList();
                return;
            }
        }

    }, true);



    // ---------------------------------------------------------
    // LISTEN-STEUERUNG
    // ---------------------------------------------------------
    document.addEventListener("keydown", e => {

        if (window.activeMode !== "video") return;

        e.stopImmediatePropagation();

        if (videoFocus === "list") {

            // ESC → zurück ins Input
            if (e.key === "Escape") {
                e.preventDefault();
                videoFocus = "input";
                inp.focus();
                unhighlightVideoList();
                return;
            }

            // ENTER → spielen
            if (e.key === "Enter") {
                e.preventDefault();
                const item = videoResults[videoCursor];
                if (item) {
                    videoBackendRef.playVideo(item.full);
                    closeVideoMode();
                    backend.toggleFromJS();
                }
                return;
            }

            // Pfeile
            if (e.key === "ArrowDown") {
                if (videoCursor < videoResults.length - 1)
                    videoCursor++;
                updateVideoSelection();
                e.preventDefault();
                return;
            }

            if (e.key === "ArrowUp") {
                if (videoCursor > 0)
                    videoCursor--;
                updateVideoSelection();
                e.preventDefault();
                return;
            }

            return;
        }

        // ESC → schließen (nur im Input)
        if (e.key === "Escape") {
            e.preventDefault();
            closeVideoMode();
            return;
        }

    }, true);



    // ---------------------------------------------------------
    // SHORTCUT "v " → nur im GRID-MODE sinnvoll
    // ---------------------------------------------------------
    document.addEventListener("keydown", e => {

        if (window.activeMode !== "grid") return;

        if (e.key === " ") {
            if (window.filterBuffer === "v") {
                e.preventDefault();
                e.stopImmediatePropagation();
                openVideoMode();
                return;
            }
        }
    }, true);



    // ---------------------------------------------------------
    // Maus-Hover Fokus
    // ---------------------------------------------------------
    const results = document.getElementById("videoResults");

    results.addEventListener("mouseenter", () => {
        highlightVideoList();
    });

    results.addEventListener("mouseleave", () => {
        if (videoFocus !== "list") {
            unhighlightVideoList();
        }
    });

} // initVideo







// =====================================================
// VIDEO MODE ÖFFNEN
// =====================================================
function openVideoMode() {

    setActiveMode("video");

    videoFocus = "input";
    videoResults = [];
    videoCursor = 0;

    document.getElementById("appgrid").style.display = "none";
    document.getElementById("videoPanel").classList.remove("hidden");

    window.filterBuffer = "";
    if (typeof applyFilter === "function") applyFilter();

    const inp = document.getElementById("videoInput");
    inp.value = "";
    inp.focus();
    setTimeout(() => inp.focus(), 30);

    renderVideoResults();
}



// =====================================================
// VIDEO MODE SCHLIESSEN
// =====================================================
function closeVideoMode() {

    setActiveMode("grid");

    videoFocus = "input";

    document.getElementById("videoPanel").classList.add("hidden");
    document.getElementById("appgrid").style.display = "grid";

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
function renderVideoResults() {
    const out = document.getElementById("videoResults");
    out.innerHTML = "";

    videoResults.forEach((item, index) => {

        const div = document.createElement("div");
        div.className = "video-item";
        div.dataset.index = index;
        div.textContent = item.short;

        // Click = Play
        div.addEventListener("click", () => {
            videoBackendRef.playVideo(item.full);
            closeVideoMode();
            backend.toggleFromJS();
        });

        out.appendChild(div);
    });

    updateVideoSelection();
}



// =====================================================
// AUSWAHL UPDATEN
// =====================================================
function updateVideoSelection() {
    const items = document.querySelectorAll(".video-item");

    items.forEach((el, idx) => {
        el.classList.toggle("sel", idx === videoCursor);
    });

    if (videoFocus === "list") {
        highlightVideoList();
    } else {
        unhighlightVideoList();
    }
}



// Export
window.initVideo = initVideo;
window.openVideoMode = openVideoMode;
window.closeVideoMode = closeVideoMode;
