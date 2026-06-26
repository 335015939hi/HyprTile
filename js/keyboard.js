// keyboard.js (final, plugin-safe)

// --------------------------------------------------------
// 0) GLOBAL KEY ROUTER (Plugins können Keys reservieren)
// --------------------------------------------------------
(function () {
	if (window.HyprTileKeys) return;

	const map = new Map(); // combo -> [entry,...]
	const norm = (s) => (s || "").trim().toLowerCase();

	function eventToCombo(e) {
		const mods = [];
		if (e.ctrlKey) mods.push("ctrl");
		if (e.altKey) mods.push("alt");
		if (e.shiftKey) mods.push("shift");
		if (e.metaKey) mods.push("meta");

		// e.key (layout-abhängig) reicht für F-Tasten / Escape / Arrows.
		// Wenn du später layout-unabhängig willst: zusätzlich e.code einführen.
		return norm(mods.concat([e.key]).join("+"));
	}

	function isTypingTarget(target) {
		if (!target) return false;
		return (
			target.isContentEditable ||
			target.tagName === "INPUT" ||
			target.tagName === "TEXTAREA" ||
			target.tagName === "SELECT"
		);
	}

	function register(owner, spec) {
		const entry = {
			owner,
			combo: norm(spec.combo),
			priority: spec.priority ?? 100,          // Plugins default hoch
			exclusive: spec.exclusive ?? true,       // default: erster gewinnt
			allowInInputs: spec.allowInInputs ?? false,
			modes: spec.modes ?? null,               // null = alle
			handler: spec.handler
		};

		if (!entry.combo || typeof entry.handler !== "function") return;

		const list = map.get(entry.combo) ?? [];
		list.push(entry);
		list.sort((a, b) => (b.priority - a.priority));
		map.set(entry.combo, list);
	}

	function unregisterOwner(owner) {
		for (const [combo, list] of map.entries()) {
			const next = list.filter(e => e.owner !== owner);
			if (next.length) map.set(combo, next);
			else map.delete(combo);
		}
	}

	function dispatch(e, backend) {
		const combo = eventToCombo(e);
		const list = map.get(combo);
		if (!list || !list.length) return false;

		const mode = window.activeMode || "grid";
		const typing = isTypingTarget(e.target);

		for (const entry of list) {
			if (entry.modes && !entry.modes.includes(mode)) continue;
			if (!entry.allowInInputs && typing) continue;

			const consumed = (entry.handler(e, backend) === true);
			if (consumed) return true;

			// exklusiv heißt: wenn registriert, bekommt es niemand anderes (auch wenn handler false zurückgibt)
			if (entry.exclusive) return true;
		}

		return false;
	}

	window.HyprTileKeys = { register, unregisterOwner, dispatch };
})();


// --------------------------------------------------------
// INIT
// --------------------------------------------------------
function initKeyboard(backend) {

	// modeHandlers existieren schon in deinem Setup – sicherstellen, falls Reihenfolge mal anders ist
	window.modeHandlers = window.modeHandlers || {};

	// =====================================================================
	// 1) CORE HOTKEYS als Registry-Einträge (plugin-safe + überschreibbar)
	// =====================================================================

	// SystemMonitor (F1)
	HyprTileKeys.register("core", {
		combo: "f1",
		priority: 0,
		exclusive: true,
		handler: () => { toggleSystemMonitor(); return true; }
	});

	// Notes (F2)
	HyprTileKeys.register("core", {
		combo: "f2",
		priority: 0,
		exclusive: true,
		handler: () => { toggleNotes(backend); return true; }
	});

	// Monitor Grid (F10)
	HyprTileKeys.register("core", {
		combo: "f10",
		priority: 0,
		exclusive: true,
		handler: () => { toggleMonitorGrid(); return true; }
	});

	// Capture Panel (Print…)
	["print", "printscreen", "printscr"].forEach(k => {
		HyprTileKeys.register("core", {
			combo: k,
			priority: 0,
			exclusive: true,
			handler: () => { toggleCapturePanel(); return true; }
		});
	});


	// =====================================================================
	// 2) MASTER KEY ROUTER – Plugins/Core-Registry zuerst, dann modeHandlers
	// =====================================================================
	document.addEventListener("keydown", (e) => {

		// 2.1 Plugins + Core Hotkeys (Reservation-System)
		if (window.HyprTileKeys && HyprTileKeys.dispatch(e, backend)) {
			e.preventDefault();
			e.stopImmediatePropagation();
			return;
		}

		// 2.2 Dein bestehender Mode-Router
		const mode = window.activeMode || "grid";
		if (window.modeHandlers[mode]) {
			const consumed = window.modeHandlers[mode](e, backend);

			if (consumed === true) {
				e.preventDefault();
				e.stopImmediatePropagation();
				return;
			}
		}

	}, true);


	// =====================================================================
	// 3) GRID NAVIGATION – wie bei dir
	// =====================================================================
	window.handleGridNavigation = function (e) {

		const items = Array.from(document.querySelectorAll(".app"));
		if (!items.length) return false;

		const grid = document.getElementById("appgrid");
		const styles = window.getComputedStyle(grid);
		const columns = styles.getPropertyValue("grid-template-columns");
		const colCount = columns.split(" ").length;

		let idx = window.getSelectedIndex ? window.getSelectedIndex() : 0;
		let newIdx = idx;

		switch (e.key) {
			case "ArrowRight": newIdx = idx + 1; break;
			case "ArrowLeft":  newIdx = idx - 1; break;
			case "ArrowDown":  newIdx = idx + colCount; break;
			case "ArrowUp":    newIdx = idx - colCount; break;

			case "Enter":
				items[idx].click();
				return true;
		}

		if (newIdx >= 0 && newIdx < items.length) {
			if (window.setSelectedIndex) window.setSelectedIndex(newIdx);
		}

		return true;
	};


	// =====================================================================
	// 4) MODE HANDLERS (dein Code – nur die F1/F2/F10/Print-Duplikate entfernt)
	// =====================================================================

	// ----------------------------- GRID MODE -----------------------------
	window.modeHandlers.grid = function (e, backend) {

		// ESC → Launcher schließen, aber NUR wenn filterBuffer leer ist
		if (e.key === "Escape") {
			if (typeof filterBuffer !== "undefined" && filterBuffer.length > 0) {
				// Filter.js soll ESC bekommen → nichts tun!
				return false;
			}

			backend.toggleFromJS();
			return true;
		}

		// GRID Navigation
		const navKeys = ["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Enter"];
		if (navKeys.includes(e.key)) {
			return window.handleGridNavigation(e);
		}

		// Alles andere NICHT konsumieren → Filter.js bekommt’s
		return false;
	};


	// ----------------------------- NOTES MODE -----------------------------
	window.modeHandlers.notes = function (e, backend) {

		// ESC → Notes schließen
		if (e.key === "Escape") {
			toggleNotes(backend);
			return true;
		}

		// F-Tasten blockieren (wie bei dir)
		if (e.key.startsWith("F")) return true;

		// Alles andere durchlassen → Editor
		return false;
	};


	// ----------------------------- MUSIC MODE -----------------------------
	window.modeHandlers.music = function (e, backend) {

		if (e.key === "Escape") {
			closeMusicMode();
			return true;
		}

		if (["ArrowUp", "ArrowDown", "Enter"].includes(e.key)) {

			if (musicFocus === "input") {
				if (e.key === "ArrowDown" || e.key === "ArrowUp") {
					musicFocus = "list";
					document.getElementById("musicInput").blur();

					const list = document.getElementById("musicResults");
					list.focus();
					highlightMusicList();
				}
			}

			return false;
		}

		return false;
	};


	// ----------------------------- VIDEO MODE -----------------------------
	window.modeHandlers.video = function (e, backend) {

		if (e.key === "Escape") {
			closeVideoMode();
			return true;
		}

		return false;
	};


	// -------------------------- SYSTEM MONITOR MODE -----------------------
	window.modeHandlers.sysmon = function (e) {
		if (e.key === "Escape") {
			toggleSystemMonitor();
			return true;
		}
		return true; // alles andere blocken
	};


	// --------------------------- CAPTURE PANEL MODE -----------------------
	window.modeHandlers.capture = function (e) {
		if (e.key === "Escape") {
			toggleCapturePanel();
			return true;
		}
		return true;
	};


	// --------------------------- MONITOR GRID MODE ------------------------
	window.modeHandlers.monitor = function (e) {
		if (e.key === "Escape") {
			toggleMonitorGrid();
			return true;
		}
		return true;
	};


	// --------------------------- CALENDAR MODE ----------------------------
	window.modeHandlers.calendar = function (e, backend) {

		if (e.key === "Escape") {
			if (typeof closeCalendar === "function") closeCalendar();
			return true;
		}

		if (e.key.startsWith("F")) return true;

		return false;
	};


	// --------------------------- ADD APP DIALOG MODE ----------------------
	window.modeHandlers.addApp = function (e) {

		if (e.key === "Escape") {
			document.getElementById("addAppDialog").classList.add("hidden");
			setActiveMode("grid");
			return true;
		}

		if (e.key.startsWith("F")) return true;
		return false;
	};


	// --------------------------- ICON CHOOSER MODE ------------------------
	window.modeHandlers.iconChooser = function (e) {

		if (e.key === "Escape") {
			document.getElementById("bigIconChooser").classList.add("hidden");
			setActiveMode("addApp");
			return true;
		}

		if (e.key.startsWith("F")) return true;
		return false;
	};


	// --------------------------- MONITOR ADD URL DIALOG MODE --------------
	window.modeHandlers.monitorAdd = function (e) {

		if (e.key === "Escape") {
			document.getElementById("monitor-add-dialog").classList.add("hidden");
			setActiveMode("monitor");
			return true;
		}

		return false;
	};


	// --------------------------- SETTINGS MODE ----------------------------
	window.modeHandlers.settings = function (e) {

		if (e.key === "Escape") {
			closeSettings();
			return true;
		}

		if (e.key.startsWith("F")) return true;
		return false;
	};
}


// --------------------------------------------------------
// EXPORT
// --------------------------------------------------------
window.initKeyboard = initKeyboard;
