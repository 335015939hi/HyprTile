function initVolume(backend) {

    // Backend → UI update
    document.addEventListener("volumeUpdated", e => {
        updateVolumeUI(e.detail);
    });

    // Mute Icon
    document.getElementById("volMute").onclick = () =>
        backend.volumeToggleMute();

    const volContainer = document.getElementById("volBarContainer");

    // CLICK-TO-SET-VOLUME
    volContainer.addEventListener("click", e => {
		const rect = volContainer.getBoundingClientRect();
		const x = e.clientX - rect.left;

		// 0.0 → 1.0
		let raw = x / rect.width;

		// in Steps von 5% (0–20)
		let step = Math.round(raw * 20);

		// zu Prozent zurück
		let percent = step * 5;
		if (percent < 0) percent = 0;
		if (percent > 100) percent = 100;

		console.log("CLICK volume step:", step, "=>", percent + "%");

		// Sofort UI updaten (optional)
		document.getElementById("volBar").style.width = percent + "%";

		// Backend setzen
		backend.setVolume(percent);
	});


    // Scrollrad für Lautstärke
    const volArea = document.getElementById("volControls");

    volArea.addEventListener("wheel", e => {
        e.preventDefault();
        if (e.deltaY < 0) backend.volumeUpSmall();
        else backend.volumeDownSmall();
    });
}


function updateVolumeUI(v) {
    const pct = Math.round(v * 100);
    document.getElementById("volBar").style.width = pct + "%";
}

// global machen
window.initVolume = initVolume;
