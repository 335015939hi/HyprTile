//
// monitor-grid.js — angepasst an das globale Modus-System
//

// Ref auf Backend
let monitorBackend = null;

function initMonitorGrid(backendRef) {
    monitorBackend = backendRef;
}



// ============================================================
// MONITOR-GRID ÖFFNEN / SCHLIESSEN
// ============================================================
function toggleMonitorGrid() {

    if (!monitorBackend) return;

    // Wenn wir NICHT im monitor-Modus sind → öffnen
    if (window.activeMode !== "monitor") {
        openMonitorGrid();
        return;
    }

    // Wenn wir im Monitor-Modus sind → schließen
    closeMonitorGrid();
}



function openMonitorGrid() {

    setActiveMode("monitor");

    const el = document.getElementById("monitor-grid");
    el.classList.remove("hidden");

    loadMonitorGrid();
    loadUrlList();

}



function closeMonitorGrid() {

    setActiveMode("grid");

    document.getElementById("monitor-grid").classList.add("hidden");

    // Dialog sicher schließen
    document.getElementById("monitor-add-dialog").classList.add("hidden");
}




// ============================================================
// GRID LADEN
// ============================================================
function loadMonitorGrid() {

    monitorBackend.loadLatestMonitorEvents().then(arr => {

        const out = document.getElementById("monitor-grid-inner");
        out.innerHTML = "";

        arr.forEach(ev => {

            const div = document.createElement("div");
            div.className = "monitor-item" + (ev.http >= 400 ? " err" : "");

            let ago = timeAgo(ev.ts);

            div.innerHTML = `
                <img src="data:image/webp;base64,${ev.thumb}">
                <div class="mtitle">${ev.url}</div>
                <div class="mmeta">
                    <b>${ev.http}</b> — ${ago}<br>
                    Total: ${ev.total}ms<br>
                    TTFB: ${ev.ttfb}ms<br>
                    DOMReady: ${ev.domready}ms
                </div>
                <canvas class="mhistory"></canvas>
            `;

            out.appendChild(div);

            // Linie rendern
            const canvas = div.querySelector(".mhistory");
            renderSparkline(canvas, ev.history);
        });
    });
}



function renderSparkline(canvas, hist) {

    if (!hist || hist.length === 0) return;

    const ctx = canvas.getContext("2d");

    const w = 200;
    const h = 40;

    canvas.width = w;
    canvas.height = h;
    ctx.clearRect(0, 0, w, h);

    const min = Math.min(...hist);
    const max = Math.max(...hist);
    const range = Math.max(1, max - min);

    // Farbe nach Qualität
    const avg = hist.reduce((a,b)=>a+b,0) / hist.length;

    let color = "rgba(0,255,150,0.9)"; // grün
    if (avg > 500)  color = "rgba(255,180,0,0.9)";  
    if (avg > 900)  color = "rgba(255,80,80,0.9)";  

    ctx.beginPath();
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;

    hist.forEach((v, i) => {
        const x = (i / (hist.length - 1)) * w;
        const y = h - ((v - min) / range) * h;

        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
    });

    ctx.stroke();
}



function timeAgo(ts) {
    let sec = Math.floor(Date.now()/1000) - ts;
    if (sec < 60) return sec + "s ago";
    let m = Math.floor(sec/60);
    if (m < 60) return m + "m ago";
    let h = Math.floor(m/60);
    return h + "h ago";
}



// ============================================================
// URL-Liste
// ============================================================
function loadUrlList() {

    monitorBackend.listMonitorUrls().then(arr => {

        const out = document.getElementById("monitor-url-list");
        out.innerHTML = "";

        arr.forEach(item => {

            const div = document.createElement("div");
            div.className = "url-entry";

            div.innerHTML = `
                <span>${item.url}</span>
                <button class="url-del" data-url="${item.url}">✕</button>
            `;

            out.appendChild(div);
        });

        // Delete
        out.querySelectorAll(".url-del").forEach(btn => {
			btn.onclick = async () => {
				const u = btn.dataset.url;

				const ok = await uiConfirm(
					`Remove monitoring for:<br><b>${u}</b>?`,
					"Remove URL"
				);

				if (!ok) return;

				monitorBackend.deleteMonitorUrl(u);
				loadUrlList();
				loadMonitorGrid();
			};
		});
    });
}



// ============================================================
// AUTO-REFRESH (nur wenn Modus = monitor)
// ============================================================
setInterval(() => {

    if (window.activeMode === "monitor") {
        loadMonitorGrid();
    }

}, 5 * 60 * 1000); // 5 min



// ------------------------------------------------------------
// Add-Dialog
// ------------------------------------------------------------
document.getElementById("monitor-add-btn").onclick = () => {
	setActiveMode("monitorAdd");
    document.getElementById("monitor-add-dialog").classList.remove("hidden");
};

document.getElementById("mad-cancel").onclick = () => {
    document.getElementById("monitor-add-dialog").classList.add("hidden");
    setActiveMode("monitor");
};

document.getElementById("mad-save").onclick = () => {

    let url = document.getElementById("mad-url").value.trim();
    if (!url) return;

    if (!url.startsWith("http")) url = "https://" + url;

    monitorBackend.addMonitorUrl(url);

    document.getElementById("monitor-add-dialog").classList.add("hidden");
    setActiveMode("monitor");

    loadUrlList();
};

document.getElementById("monitor-sidebar-toggle").onclick = () => {
    document.getElementById("monitor-sidebar").classList.toggle("collapsed");
};


// Export
window.initMonitorGrid = initMonitorGrid;
window.toggleMonitorGrid = toggleMonitorGrid;
