function initClock() {
    function updateClock() {
        const dt = new Date();

        const options = {
            weekday: "long",
            year: "numeric",
            month: "long",
            day: "numeric"
        };

        const dateText = dt.toLocaleDateString("en-US", options);
        const timeText = dt.toLocaleTimeString("en-US", { hour12: false });

        document.getElementById("datetimebar").textContent = dateText + " – " + timeText;
            
    }

    updateClock();
    setInterval(updateClock, 1000);
}
window.initClock = initClock;
