//
// calendar.js — angepasst an das globale Modus-System
//

let currentMonth = new Date().getMonth();
let currentYear = new Date().getFullYear();

let currentEventId = -1;
let currentEventDate = "";
window.allEvents = {};  // globale Event-Sammlung


// =====================================================
// INITIALISIERUNG
// =====================================================
function initCalendar() {

    const datetimeBar = document.getElementById("datetimebar");

    datetimeBar.onclick = () => {

        if (window.activeMode === "calendar") {
            closeCalendar();
            return;
        }

        openCalendar();
    };
}



// =====================================================
// KALENDER ÖFFNEN / SCHLIESSEN
// =====================================================
function openCalendar() {

    setActiveMode("calendar");

    const cal = document.getElementById("calendar");
    const grid = document.getElementById("appgrid");

    loadCalendar(currentYear, currentMonth);

    cal.classList.add("visible");
    grid.style.display = "none";
}


function closeCalendar() {

    const cal = document.getElementById("calendar");
    const grid = document.getElementById("appgrid");

    cal.classList.remove("visible");
    grid.style.display = "grid";

    setActiveMode("grid");
}



// =====================================================
// EVENTS LADEN + KALENDER BAUEN
// =====================================================
function loadCalendar(year, month) {

    backend.loadEventsForMonth(year, month + 1).then(events => {
        console.log("LOADCALENDAR → Parameter (JS):", { year, month });
        console.log("LOADCALENDAR → Parameter (C++):", { year, realMonth: month + 1 });
        console.log("LOADCALENDAR → Events vom Backend:", events);

        window.allEvents = {};

        events.forEach(ev => {

			// 🔥 Event-Objekt auf einheitliches Format bringen
			const ne = normalizeEvent(ev);

			const d = ne.date;
			if (!window.allEvents[d]) window.allEvents[d] = [];
			window.allEvents[d].push(ne);
		});

        console.log("ALL EVENTS BUILT:", window.allEvents);

        buildCalendar(year, month);
    });
}



// =====================================================
// KALENDER RENDERN
// =====================================================
function buildCalendar(year, month) {

    const cal = document.getElementById("calendar");
    cal.innerHTML = "";

    const monthName = new Date(year, month)
        .toLocaleString("en-US", { month: "long" });

    // ---------- Header ----------
    const header = document.createElement("div");
    header.className = "cal-header";
    header.innerHTML = `
        <span id="calPrev" style="cursor:pointer">◀</span>
        <span>${monthName} ${year}</span>
        <span id="calNext" style="cursor:pointer">▶</span>
    `;
    cal.appendChild(header);

    // ---------- Grid ----------
    const grid = document.createElement("div");
    grid.className = "cal-grid";

    const dayNames = ["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"];
    dayNames.forEach(name => {
        const e = document.createElement("div");
        e.className = "day-name";
        e.textContent = name;
        grid.appendChild(e);
    });

    // ---------- Startversatz ----------
    let first = new Date(year, month, 1);
    let start = first.getDay();
    if (start === 0) start = 7;

    for (let i = 1; i < start; i++) {
        grid.appendChild(document.createElement("div"));
    }

    // ---------- Tage rendern ----------
    const daysInMonth = new Date(year, month + 1, 0).getDate();
    const today = new Date();

    for (let d = 1; d <= daysInMonth; d++) {

        const cell = document.createElement("div");
        cell.className = "day";
        cell.textContent = d;

        const mm = String(month + 1).padStart(2, "0");
        const dd = String(d).padStart(2, "0");
        const dateKey = `${year}-${mm}-${dd}`;
        cell.dataset.fullDate = dateKey;

        // Heute markieren
        if (
            d === today.getDate() &&
            year === today.getFullYear() &&
            month === today.getMonth()
        ) {
            cell.classList.add("today");
        }

        // Event vorhanden?
        if (window.allEvents[dateKey]) {
            cell.classList.add("event-day");
        }

        grid.appendChild(cell);
    }

    cal.appendChild(grid);

    // ---------- Navigation ----------
    document.getElementById("calPrev").onclick = () => {
        month--;
        if (month < 0) {
            month = 11;
            year--;
        }
        currentYear = year;
        currentMonth = month;
        loadCalendar(year, month);
    };

    document.getElementById("calNext").onclick = () => {
        month++;
        if (month > 11) {
            month = 0;
            year++;
        }
        currentYear = year;
        currentMonth = month;
        loadCalendar(year, month);
    };
}



// =====================================================
// KLICK INS KALENDERFELD
// =====================================================
document.addEventListener("click", e => {
    if (!e.target.classList.contains("day")) return;

    e.stopPropagation();        // 🛑 <<< DAS FÜGT ALLES
    e.stopImmediatePropagation();

    const date = e.target.dataset.fullDate;
    const events = window.allEvents[date] || [];

    if (events.length === 0) {
        openEventDialog(date, null);
        return;
    }

    if (events.length === 1) {
        openEventDialog(date, events[0]);
        return;
    }

    showEventsForDay(date);
});





// =====================================================
// EVENT-DIALOG ÖFFNEN
// =====================================================
function openEventDialog(date, event = null) {
	console.log("OPEN EVENT DIALOG →", { date, event });
    currentEventDate = date;

    const dlg = document.getElementById("eventDialog");
    dlg.classList.add("visible");

    document.getElementById("eventTitle").value = event ? event.title : "";
    document.getElementById("eventAllDay").checked = event ? event.allday : true;
    document.getElementById("eventRepeat").checked = event ? event.repeat : false;

    if (event) {
        document.getElementById("eventStart").value = event.start;
        document.getElementById("eventEnd").value = event.end;
        currentEventId = event.id;
    } else {
        currentEventId = -1;
        document.getElementById("eventStart").value = "";
        document.getElementById("eventEnd").value = "";
    }
}



// =====================================================
// EVENT SPEICHERN
// =====================================================
document.getElementById("eventSave").onclick = async () => {

    const titleInput = document.getElementById("eventTitle");
    const title = titleInput.value.trim();

    // ❌ Titel fehlt → Hinweis anzeigen
    if (!title) {
        await uiAlert(
            "An event must have a title.",
            "Missing title"
        );
        setTimeout(() => {
			titleInput.classList.add("error");
			setTimeout(() => titleInput.classList.remove("error"), 1200);
			titleInput.focus();
		}, 100);
        return;
    }

    // ✅ speichern wie bisher
    backend.saveEvent(
        currentEventId,
        currentEventDate,
        title,                    // ← bereinigter Titel
        eventAllDay.checked,
        eventStart.value,
        eventEnd.value,
        eventRepeat.checked
    ).then(() => {

        document.getElementById("eventDialog").classList.remove("visible");
        loadCalendar(currentYear, currentMonth);
    });
};



// =====================================================
// EVENT LÖSCHEN
// =====================================================
document.getElementById("eventDelete").onclick = () => {

    if (currentEventId === -1) return;

    backend.deleteEvent(currentEventId).then(() => {
        document.getElementById("eventDialog").classList.remove("visible");
        loadCalendar(currentYear, currentMonth);
    });
};



// =====================================================
// EVENT CANCEL  ← **FEHLTE BISHER**
// =====================================================
document.getElementById("eventCancel").onclick = () => {
    document.getElementById("eventDialog").classList.remove("visible");
};



// =====================================================
// EVENT-POPUP (derzeit alert)
// =====================================================
function showEventsForDay(date) {

    const ev = window.allEvents[date] || [];

    if (ev.length === 0) {
        alert(`Keine Events am ${date}`);
        return;
    }

    let msg = `Events am ${date}:\n\n`;

    ev.forEach(e => {
        msg += "• " + e.title;
        if (!e.allday) msg += ` (${e.start}–${e.end})`;
        msg += "\n";
    });

    uiAlert(
		msg.replace(/\n/g, "<br>"),
		"Events"
	);
}

function normalizeEvent(ev) {
    return {
        id: ev.id,
        date: ev.date,
        title: ev.title,

        // Boolean-Felder sauber normalisieren
        allday: ev.allday ?? ev.all_day ?? false,

        // Start/End normalisieren
        start: ev.start ?? ev.start_time ?? "",
        end: ev.end ?? ev.end_time ?? "",

        // Wiederholung normalisieren
        repeat: ev.repeat ?? ev.repeat_yearly ?? false
    };
}

// =====================================================
// GLOBAL EXPORT
// =====================================================
window.initCalendar = initCalendar;
window.openCalendar = openCalendar;
window.closeCalendar = closeCalendar;
