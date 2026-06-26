(function() {

    const debugDiv = document.getElementById("debugConsole");

    function writeToConsole(type, args) {
        if (!debugDiv) return;

        const msg = [...args].map(a => 
            (typeof a === "object" ? JSON.stringify(a, null, 2) : a)
        ).join(" ");

        const line = document.createElement("div");
        line.textContent = `[${type.toUpperCase()}] ${msg}`;
        debugDiv.appendChild(line);

        debugDiv.scrollTop = debugDiv.scrollHeight;
    }

    // Originalfunktionen sichern
    const _log   = console.log;
    const _warn  = console.warn;
    const _error = console.error;

    // Überschreiben
    console.log = function(...args) {
        _log.apply(console, args);
        writeToConsole("log", args);
    };

    console.warn = function(...args) {
        _warn.apply(console, args);
        writeToConsole("warn", args);
    };

    console.error = function(...args) {
        _error.apply(console, args);
        writeToConsole("error", args);
    };

    // Konsole ein/aus per F12
    document.addEventListener("keydown", e => {
        if (e.key === "F12") {
            debugDiv.style.display =
                (debugDiv.style.display === "none") ? "block" : "none";
        }
    });

})();
