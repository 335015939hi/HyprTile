function initCapture(backend) {

    document.addEventListener("toggleCapture", () => toggleCapturePanel());

    let captureDelay = 3;

    const audioCheckbox = document.getElementById("captureAudio");

    // Delay Buttons
    document.querySelectorAll(".delay").forEach(d => {
        d.addEventListener("click", () => {
            captureDelay = parseInt(d.dataset.delay);
            const parentGroup = d.closest(".delay-buttons");
            parentGroup.querySelectorAll(".delay")
                .forEach(btn => btn.classList.remove("active"));
            d.classList.add("active");
        });
    });

    // Aufnahme starten
    document.querySelectorAll("#capturePanel .btn").forEach(btn => {
        btn.addEventListener("click", () => {
            const action = btn.dataset.action;

            if (action.startsWith("rec-")) {
                document.getElementById("stopRecordingBtn")
                    .classList.remove("versteckt");
            }

            backend.capture(
                action,
                captureDelay,
                audioCheckbox?.checked === true 
            );
        });
    });

    document.getElementById("stopRecordingBtn").onclick = () => {
        document.getElementById("stopRecordingBtn")
            .classList.add("versteckt");
        backend.stopRecording();
    };

    // Default Delay = 3s
    document.querySelectorAll(".delay-buttons").forEach(group => {
        const b = group.querySelector('.delay[data-delay="3"]');
        if (b) b.classList.add("active");
    });
}




function toggleCapturePanel() {
    const panel = document.getElementById("capturePanel");
    const grid  = document.getElementById("appgrid");

    const nowHidden = panel.classList.toggle("hidden");

    // UI umschalten
    grid.style.display = nowHidden ? "grid" : "none";

    // ⭐⭐⭐ Modus setzen:
    // wenn sichtbar → capture mode
    // wenn versteckt → grid mode
    window.setMode(nowHidden ? "grid" : "capture");
}



// globale Sichtbarkeit
window.initCapture = initCapture;
window.toggleCapturePanel = toggleCapturePanel;
