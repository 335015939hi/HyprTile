// Globale Variablen
window.appsCache = [];
window.selectedIndex = 0;

function initApps(backend) {
    document.addEventListener("appsUpdated", e => {
        window.appsCache = e.detail;
        renderApps(window.appsCache, backend);
    });
}

function generateUniqId() {
    return "app_" + Math.random().toString(36).substr(2, 9);
}

function renderApps(apps, backend) {
    const grid = document.getElementById("appgrid");
    grid.innerHTML = "";

    apps.forEach(app => {
        const div = document.createElement("div");
        div.className = "app";
        div.dataset.id = app.id;

        div.innerHTML = `
            <div class="delete-btn">×</div>
            <div class="inner">
                <img src="${app.icon}" style="width:128px;height:128px;">
                <div>${app.name}</div>
            </div>
        `;

        // ❌ DELETE CLICK
        div.querySelector(".delete-btn").addEventListener("click", ev => {
            ev.preventDefault();
            ev.stopPropagation();

            console.log("DELETE CLICKED for ID:", app.id);

            div.draggable = false;

            deleteApp(app.id, backend);
        });

        // Normaler Launch
        div.onclick = () => spinAndLaunch(div, app.id, backend);

        grid.appendChild(div);
    });

    window.selectedIndex = 0;
    updateSelected();
}



function deleteApp(id, backend) {

    // 1. Aus UI entfernen
    const el = document.querySelector(`.app[data-id="${id}"]`);
    if (el) el.remove();

    // 2. appsCache aktualisieren
    window.appsCache = window.appsCache.filter(a => a.id !== id);

    // 3. Backend speichern
    const ids = window.appsCache.map(a => a.id);
    backend.saveReorderedApps(ids);

    // 4. UI neu rendern
    renderApps(window.appsCache, backend);
}



function updateSelected() {
    const items = document.querySelectorAll(".app");
    items.forEach(el => el.classList.remove("selected"));

    if (items[window.selectedIndex])
        items[window.selectedIndex].classList.add("selected");
}



function spinAndLaunch(el, id, backend) {
    el.classList.remove("spin");
    void el.offsetWidth;
    el.classList.add("spin");

    setTimeout(() => el.classList.remove("spin"), 250);

    setTimeout(() => backend.launch(id), 250);
}



function setSelectedIndex(idx) {
    window.selectedIndex = idx;
    updateSelected();
}

function getSelectedIndex() {
    return window.selectedIndex;
}



/* ===================================================================
   ADD APP DIALOG — jetzt mit Modussteuerung
   =================================================================== */

function openAddAppDialog(backend) {

    const dialog = document.getElementById("addAppDialog");
    const cmdInput = document.getElementById("aad-cmd");

    let chosenIcon = "icons/Logos/question-fill.svg";

    // Modus setzen
    setActiveMode("addApp");

    dialog.classList.remove("hidden");
    cmdInput.value = "";

    document.getElementById("aad-cancel").onclick = () => {
        dialog.classList.add("hidden");
        setActiveMode("grid");
    };

    // ICON CHOOSER
    document.getElementById("aad-icon").onclick = (ev) => {
        ev.preventDefault();
        ev.stopPropagation();

        // Add-App-Fenster verstecken
        dialog.classList.add("hidden");

        // Icon-Chooser öffnen
        openBigIconChooser((iconPath) => {

            chosenIcon = iconPath;
            document.getElementById("aad-icon").textContent = iconPath;

            dialog.classList.remove("hidden");

            // zurück in AddApp-Modus
            setActiveMode("addApp");
        });
    };

    // SAVE
    document.getElementById("aad-save").onclick = () => {

        const nameInput = document.getElementById("aad-name");
        const cmdInput  = document.getElementById("aad-cmd");

        const name = nameInput.value.trim();
        const cmd  = cmdInput.value.trim();

        if (!name) return uiAlert("Please enter a name.", "Add Application");
		if (!cmd)  return uiAlert("Command is required.", "Add Application");

        const uniqid = generateUniqId();

        const newApp = {
            id: uniqid,
            name: name,
            cmd: cmd,
            icon: chosenIcon
        };

        window.appsCache.push(newApp);

        backend.saveFullApps(window.appsCache);

        renderApps(window.appsCache, backend);

        dialog.classList.add("hidden");

        // zurück in normalen Modus
        setActiveMode("grid");
    };
}




/* ===================================================================
   ICON CHOOSER (unverändert)
   =================================================================== */

async function openIconChooser(onSelect) {

    const chooser = document.getElementById("iconChooser");
    const list = document.getElementById("iconList");
    const closeBtn = document.getElementById("ic-close");

    list.innerHTML = "";

    const icons = await backend.listIcons();

    icons.forEach(path => {
        const img = document.createElement("img");
        img.src = path;

        img.onclick = () => {
            chooser.classList.add("hidden");
            onSelect(path);
        };

        list.appendChild(img);
    });

    closeBtn.onclick = () => chooser.classList.add("hidden");

    chooser.classList.remove("hidden");
}



/* ===================================================================
   BIG ICON CHOOSER (verändert: Modus bleibt "addApp")
   =================================================================== */

async function openBigIconChooser(onSelect) {

    const chooser = document.getElementById("bigIconChooser");
    const close = document.getElementById("bic-close");
    const list = document.getElementById("bigIconList");

    // Modus: Icon chooser aktiv
    setActiveMode("iconChooser");

    chooser.classList.remove("hidden");

    await new Promise(r => requestAnimationFrame(r));

    list.innerHTML = "";
    list.style.position = "relative";
    list.style.overflowY = "auto";

    const TILE_W = 120;
    const TILE_H = 140;
    const VISIBLE_COUNT = 200;

    const allIcons = await backend.listIconsLimited(50000);
    const total = allIcons.length;

    function getColumns() {
        return Math.max(2, Math.floor(list.clientWidth / TILE_W));
    }

    let columns = getColumns();
    let totalRows = Math.ceil(total / columns);

    const spacer = document.createElement("div");
    spacer.style.width = "1px";
    spacer.style.height = (totalRows * TILE_H) + "px";
    list.appendChild(spacer);

    const pool = [];
    for (let i = 0; i < VISIBLE_COUNT; i++) {
        const img = document.createElement("img");
        img.style.position = "absolute";
        img.style.width = "100px";
        img.style.height = "100px";
        img.style.padding = "10px";
        img.style.cursor = "pointer";

        img.onclick = () => {

            chooser.classList.add("hidden");

            // zurück in AddApp-Modus
            setActiveMode("addApp");

            onSelect(img.dataset.path);
        };

        list.appendChild(img);
        pool.push(img);
    }

    function updateVisible() {
        const scrollTop = list.scrollTop;
        const firstRow = Math.floor(scrollTop / TILE_H);
        const startIndex = firstRow * columns;

        for (let i = 0; i < VISIBLE_COUNT; i++) {
            const img = pool[i];
            const iconIndex = startIndex + i;

            if (iconIndex >= total) {
                img.style.display = "none";
                continue;
            }

            const row = Math.floor(iconIndex / columns);
            const col = iconIndex % columns;

            img.style.display = "block";
            img.style.transform =
                `translate(${col * TILE_W}px, ${row * TILE_H}px)`;

            img.src = allIcons[iconIndex];
            img.dataset.path = allIcons[iconIndex];
        }
    }

    list.onscroll = updateVisible;

    window.addEventListener("resize", () => {
        const old = columns;
        columns = getColumns();
        if (columns !== old) {
            totalRows = Math.ceil(total / columns);
            spacer.style.height = (totalRows * TILE_H) + "px";
            updateVisible();
        }
    });

    updateVisible();

    close.onclick = () => {
        chooser.classList.add("hidden");

        // zurück in Add-App-Modus
        setActiveMode("addApp");
    };
}




/* Globale API */
window.initApps = initApps;
window.renderApps = renderApps;
window.setSelectedIndex = setSelectedIndex;
window.getSelectedIndex = getSelectedIndex;
