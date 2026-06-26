let noteEditor = null;
let isEditing = false;
let currentNoteId = null;

function initNotes(backend) {

    document.addEventListener("toggleNotes", () => toggleNotes(backend));

    document.getElementById("noteIcon").onclick = () =>
        toggleNotes(backend);

    document.getElementById("newNoteBtn").onclick = () => {
        backend.createNote().then(id => {
            loadNotesList(backend);
            openNoteEditor(id, backend);
        });
    };

    document.getElementById("saveNoteBtn").onclick = () => {
        saveNote(backend);
    };

    document.getElementById("deleteNoteBtn").onclick = () => {
        if (!currentNoteId) return;

        backend.deleteNote(currentNoteId).then(() => {
            currentNoteId = null;
            document.getElementById("noteTitle").value = "";
            document.getElementById("noteContent").value = "";
            loadNotesList(backend);
        });
    };

    document.getElementById("editNoteBtn").onclick = () => beginEditing();

    document.getElementById("noteContentView").addEventListener("click", e => {
        const a = e.target.closest("a");
        if (!a) return;

        e.preventDefault();
        const url = a.getAttribute("href");
        if (!url) return;

        backend.openExternalUrl(url);

        if (backend.toggleFromJS)
            backend.toggleFromJS();
    });
}



function toggleNotes(backend) {
    const panel = document.getElementById("notesPanel");
    const grid = document.getElementById("appgrid");

    const isVisible = panel.classList.toggle("visible");

    // UI umschalten
    grid.style.display = isVisible ? "none" : "grid";

    // ⭐⭐⭐ NEU: zentraler Moduswechsel
    window.setMode(isVisible ? "notes" : "grid");

    if (isVisible) {
        // Filter ausschalten
        if (typeof filterBuffer !== "undefined") {
            filterBuffer = "";
            applyFilter();
        }

        loadNotesList(backend);
    }
}



function loadNotesList(backend) {
    const list = document.getElementById("notesEntries");
    list.innerHTML = "";

    backend.loadNotes().then(notes => {
        notes.forEach(n => {
            const d = document.createElement("div");
            d.className = "note-item";
            d.textContent = n.title || "(Untitled)";
            d.onclick = () => openNoteEditor(n.id, backend);
            list.appendChild(d);
        });
    });
}



function openNoteEditor(id, backend) {
    backend.loadNote(id).then(note => {
        currentNoteId = id;

        // Editor zerstören wenn aktiv
        if (noteEditor) {
            $('#noteContent').trumbowyg('destroy');
            noteEditor = null;
        }
        isEditing = false;

        // Titel
        document.getElementById("noteTitle").value = note.title || "";

        // Anzeige-Modus
        document.getElementById("noteContentView").innerHTML = note.content || "";
        document.getElementById("noteContentView").style.display = "block";

        // Textarea vorbereiten
        const textarea = document.getElementById("noteContent");
        textarea.value = note.content || "";
        textarea.style.display = "none";

        // Save/Delete Buttons verstecken
        document.querySelector(".note-editor-actions").style.display = "none";

        highlightActive(id);
    });
}



function highlightActive(id) {
    document.querySelectorAll(".note-item").forEach(el =>
        el.classList.remove("active")
    );

    document.querySelectorAll(".note-item").forEach(el => {
        if (el.textContent === document.getElementById("noteTitle").value)
            el.classList.add("active");
    });
}



function beginEditing() {
    if (isEditing) return;
    isEditing = true;

    const view = document.getElementById("noteContentView");
    const textarea = document.getElementById("noteContent");

    textarea.value = view.innerHTML;

    view.style.display = "none";
    textarea.style.display = "block";

    document.querySelector(".note-editor-actions").style.display = "flex";

    if (noteEditor) {
        $('#noteContent').trumbowyg('destroy');
        noteEditor = null;
    }

    noteEditor = $('#noteContent').trumbowyg({
        lang: 'en',
        btns: [
            ['viewHTML'],
            ['undo', 'redo'],
            ['strong', 'em', 'del'],
            ['base64'],
            ['link'],
            ['unorderedList', 'orderedList'],
            ['horizontalRule'],
            ['removeformat']
        ]
    });
}



function saveNote(backend) {

    let finalHTML = "";

    if (isEditing) {
        finalHTML = $('#noteContent').trumbowyg('html');

        $('#noteContent').trumbowyg('destroy');
        noteEditor = null;
        isEditing = false;
    } else {
        finalHTML = document.getElementById("noteContent").value;
    }

    document.getElementById("noteContentView").innerHTML = finalHTML;
    document.getElementById("noteContentView").style.display = "block";
    document.getElementById("noteContent").style.display = "none";

    document.querySelector(".note-editor-actions").style.display = "none";

    backend.saveNote(
        currentNoteId,
        document.getElementById("noteTitle").value,
        finalHTML
    ).then(() => loadNotesList(backend));
}



/* Globale Sichtbarkeit */
window.initNotes = initNotes;
window.toggleNotes = toggleNotes;
