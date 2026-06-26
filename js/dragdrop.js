// Globale Zustände
let editMode = false;
let dragSrc = null;

function initDragDrop(backend) {

    // ============================================================
    // CTRL → Drag/Drop einschalten (nur im GRID-MODE!)
    // ============================================================
    document.addEventListener("keydown", e => {

        if (e.key === "Control") {

            // Nur erlauben, wenn wir im grid-Modus sind
            if (window.activeMode !== "grid") {
                return;
            }

            if (!editMode) {
                editMode = true;
                document.body.classList.add("edit-mode");
                enableDrag(true);
            }
        }
    });


    // ============================================================
    // CTRL loslassen → Drag/Drop beenden
    // ============================================================
    document.addEventListener("keyup", e => {
        if (e.key === "Control") {
            if (editMode) {
                editMode = false;
                document.body.classList.remove("edit-mode");
                enableDrag(false);
            }
        }
    });



    // ============================================================
    //  Wenn der MODE wechselt, immer Drag&Drop ausschalten !!!
    // ============================================================
    document.addEventListener("modechange", () => {
        // Sobald wir NICHT im grid-Modus sind → editMode aus!
        if (window.activeMode !== "grid") {
            if (editMode) {
                editMode = false;
                document.body.classList.remove("edit-mode");
                enableDrag(false);
            }
        }
    });



    // ============================================================
    //  DRAG START — nur wenn NICHT delete-btn
    // ============================================================
    document.addEventListener("dragstart", e => {

        if (!editMode) return;

        // Niemals Drag vom Delete-Button
        if (e.target.closest(".delete-btn")) {
            console.log("DRAGSTART BLOCKED on delete-btn");
            e.preventDefault();
            e.stopPropagation();
            return false;
        }

        dragSrc = e.target.closest(".app");
        if (!dragSrc) return;

        e.dataTransfer.effectAllowed = "move";
        e.dataTransfer.setData("text/plain", dragSrc.dataset.id);

        // Ghost entfernen (unsichtbares Drag-Bild)
        const ghost = document.createElement("div");
        ghost.style.opacity = "0";
        document.body.appendChild(ghost);
        e.dataTransfer.setDragImage(ghost, 0, 0);
        setTimeout(() => ghost.remove(), 0);
    });



    // ============================================================
    //  DRAG OVER — nur im Edit-Mode
    // ============================================================
    document.addEventListener("dragover", e => {
        if (editMode) e.preventDefault();
    });



    // ============================================================
    //  DROP — Elemente neu sortieren
    // ============================================================
    document.addEventListener("drop", e => {

        if (!editMode) return;
        e.preventDefault();

        const target = e.target.closest(".app");
        if (!target || target === dragSrc) return;

        const grid = document.getElementById("appgrid");
        const children = Array.from(grid.children);

        const srcIndex = children.indexOf(dragSrc);
        const dstIndex = children.indexOf(target);

        if (srcIndex < dstIndex)
            grid.insertBefore(dragSrc, target.nextSibling);
        else
            grid.insertBefore(dragSrc, target);

        saveNewOrder(backend);
    });

}



function enableDrag(enable) {
    document.querySelectorAll(".app").forEach(el => {

        // Delete-Button niemals dragbar
        const del = el.querySelector(".delete-btn");
        if (del) {
            del.draggable = false;
            del.style.pointerEvents = "auto";

            del.addEventListener("mousedown", ev => {
                ev.stopPropagation();
                ev.preventDefault();
            });

            del.addEventListener("dragstart", ev => {
                console.log("delete-btn attempted dragstart");
                ev.preventDefault();
                ev.stopPropagation();
            });
        }

        // App-Kachel dragbar machen / abschalten
        el.draggable = enable;
    });
}



function saveNewOrder(backend) {
    const ids = Array.from(document.querySelectorAll(".app"))
        .map(el => el.dataset.id);

    backend.saveReorderedApps(ids);
}



// global exportieren
window.initDragDrop = initDragDrop;
