function initSettings(backend) {

    const btn = document.getElementById("settingsButton");
    const panel = document.getElementById("settingsPanel");

    const fmInput    = document.getElementById("setFileManager");
    const audioInput = document.getElementById("setAudioPlayer");
    const videoInput = document.getElementById("setVideoPlayer");

    const musicRootInput = document.getElementById("setMusicRoot");
    const videoRootInput = document.getElementById("setVideoRoot");

    // -----------------------------
    // LADEN beim Start
    // -----------------------------
    backend.getConfigValue("file_manager", "").then(v => {
        fmInput.value = v;
    });

    backend.getConfigValue("audio_player", "vlc").then(v => {
        audioInput.value = v;
    });

    backend.getConfigValue("video_player", "vlc").then(v => {
        videoInput.value = v;
    });

    backend.getConfigValue("music_root", "").then(v => {
        musicRootInput.value = v;
    });

    backend.getConfigValue("video_root", "").then(v => {
        videoRootInput.value = v;
    });

    // -----------------------------
    // Folder-Picker Buttons
    // -----------------------------
    document.getElementById("pickMusicRoot").onclick = () => {
        backend.chooseFolder("music_root");
    };

    document.getElementById("pickVideoRoot").onclick = () => {
        backend.chooseFolder("video_root");
    };

    // -----------------------------
    // Ergebnis vom Backend empfangen
    // -----------------------------
    backend.folderChosen.connect((key, path) => {
        if (key === "music_root") {
            musicRootInput.value = path;
        }
        if (key === "video_root") {
            videoRootInput.value = path;
        }
    });

    // -----------------------------
    // Panel Buttons
    // -----------------------------
    btn.onclick = () => openSettings();
    document.getElementById("settingsClose").onclick = () => closeSettings();

    document.getElementById("settingsSave").onclick = () => {
        backend.setConfigValue("file_manager", fmInput.value.trim());
        backend.setConfigValue("audio_player", audioInput.value.trim());
        backend.setConfigValue("video_player", videoInput.value.trim());

        // music_root / video_root sind bereits gesetzt
        closeSettings();
    };

    // -----------------------------
    // Rescan Buttons
    // -----------------------------
    document.getElementById("reloadMusic").onclick = () => {
        backend.scanMusicDir();
    };

    document.getElementById("reloadVideo").onclick = () => {
        backend.scanVideoDir();
    };
}

function openSettings() {
    setActiveMode("settings");
    document.getElementById("settingsPanel").classList.add("visible");

    // Filter deaktivieren
    if (typeof filterBuffer !== "undefined") {
        filterBuffer = "";
        applyFilter();
    }

    document.getElementById("appgrid").style.display = "none";
}

function closeSettings() {
    setActiveMode("grid");
    document.getElementById("settingsPanel").classList.remove("visible");
    document.getElementById("appgrid").style.display = "grid";
}

window.initSettings  = initSettings;
window.openSettings  = openSettings;
window.closeSettings = closeSettings;
