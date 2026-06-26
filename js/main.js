window.sysmonMode = false;

window.backgroundAnimStarted = false;
window.onWindowShown = async () => {
    // Hintergrundanimation nur einmal starten
    if (!backgroundAnimStarted) {
        backgroundAnimStarted = true;
        initBackgroundAnim();
    }

    // Fällige Events prüfen (Backend filtert doppelte!)
    const events = await backend.checkDueEvents();
    events.forEach(ev => {
        uiAlert(`<h2>${ev.title}</h2>`, "Event");
    });
};

document.addEventListener("DOMContentLoaded", async () => {

    const backend = await initBackend();
    
    // gamepad kommt aus WebChannel
	gamepad.action.connect((name, pressed) => {
	  if (!pressed) return; // nur on-press navigieren

	  const mode = window.activeMode || "grid";

	  const nav = (key) => window.handleGridNavigation && window.handleGridNavigation({ key });

	  if (name === "up") return nav("ArrowUp");
	  if (name === "down") return nav("ArrowDown");
	  if (name === "left") return nav("ArrowLeft");
	  if (name === "right") return nav("ArrowRight");
	  if (name === "confirm") return nav("Enter");
	  if (name === "back") {
		// wie ESC
		return window.modeHandlers?.[mode]?.({ key: "Escape" }, backend);
	  }
	});
	
	const exitBtn = document.getElementById("exitHypr");
	if (exitBtn) {
		exitBtn.onclick = async () => {
			const ok = await uiConfirm(
				"Do you really want to exit Hyprland?",
				"Exit Hyprland"
			);

			if (ok) {
				backend.exitHyprland();
			}
		};
	}
    
	initSettings(backend);
    initApps(backend);
    initFilter();
    initKeyboard(backend);
    initDragDrop(backend);
    initVolume(backend);
    initSysmon();
    initCapture(backend);
    initCalendar();
    initNotes(backend);
    initClock();
    initMusic(backend);
    initVideo(backend);
    initMonitorGrid(backend);        // <<< Monitor initialisieren 
    
    document.getElementById("appAddBtn").onclick = () => {
		openAddAppDialog(backend);
	};
	
});


