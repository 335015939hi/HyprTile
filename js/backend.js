function initBackend() {
  return new Promise((resolve) => {

    function waitForWebChannel() {
      if (typeof qt === "undefined" || !qt.webChannelTransport || typeof QWebChannel === "undefined") {
        return setTimeout(waitForWebChannel, 50);
      }

      new QWebChannel(qt.webChannelTransport, (channel) => {
        const backend = channel.objects.backend;
        window.plugins = channel.objects.plugins;

        // -----------------------------
        // Qt -> JS Signals
        // -----------------------------
        backend.sendApps.connect((apps) => {
          document.dispatchEvent(new CustomEvent("appsUpdated", { detail: apps }));
        });

        backend.sendVolume.connect((v) => {
          document.dispatchEvent(new CustomEvent("volumeUpdated", { detail: v }));
        });

        backend.recordingStarted.connect(() => {
          document.dispatchEvent(new Event("recordingStarted"));
        });

        backend.recordingStopped.connect(() => {
          document.dispatchEvent(new Event("recordingStopped"));
        });

        // -----------------------------
        // Plugin APIs nach window spiegeln
        // -----------------------------
        for (const [name, obj] of Object.entries(channel.objects)) {
          if (name === "backend" || name === "plugins") continue;
          window[name] = obj; // überschreiben ist okay, garantiert konsistent
          // optional debug:
          console.log("🔌 Plugin exposed:", name);
        }

        // global verfügbar
        window.backend = backend;

        // -----------------------------
        // ✅ WebChannel-Ready "Latch"
        // (damit Plugins, die später injiziert werden,
        // trotzdem sofort starten können)
        // -----------------------------
        window.__webchannelReady = true;
        window.__webchannel = channel;

        document.dispatchEvent(new Event("webchannel-ready"));
        document.dispatchEvent(new Event("frontendReady")); // optional legacy, falls du es wo nutzt

        // -----------------------------
        // ✅ Jetzt erst C++ informieren
        // (damit C++ z.B. Apps senden kann)
        // -----------------------------
        backend.frontendReady();

        resolve(backend);
      });
    }

    waitForWebChannel();
  });
}

// global verfügbar machen (wichtig für main.js)
window.initBackend = initBackend;
