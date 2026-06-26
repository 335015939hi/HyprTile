function uiDialog({ title = "", content = "", confirm = false }) {
    return new Promise(resolve => {

        const dlg = document.getElementById("ui-dialog");
        const titleEl = document.getElementById("ui-dialog-title");
        const contentEl = document.getElementById("ui-dialog-content");
        const okBtn = document.getElementById("ui-dialog-ok");
        const cancelBtn = document.getElementById("ui-dialog-cancel");

        titleEl.textContent = title;
        contentEl.innerHTML = content;

        dlg.classList.remove("hidden");

        cancelBtn.style.display = confirm ? "inline-block" : "none";

        function close(result) {
            dlg.classList.add("hidden");
            okBtn.onclick = cancelBtn.onclick = null;
            resolve(result);
        }

        okBtn.onclick = () => close(true);
        cancelBtn.onclick = () => close(false);
    });
}

// Convenience Wrapper
window.uiAlert = (msg, title="Info") =>
    uiDialog({ title, content: msg, confirm: false });

window.uiConfirm = (msg, title="Confirm") =>
    uiDialog({ title, content: msg, confirm: true });
