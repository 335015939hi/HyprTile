function initStarfield() {
    const canvas = document.getElementById("animcanvas");
    const ctx = canvas.getContext("2d");

    let warpFactor = 0.30;

    function resize() {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
    }
    resize();
    window.addEventListener("resize", resize);

    const STAR_COUNT = 350;
    const stars = [];

    function resetStar(s) {
        s.x = canvas.width / 2;
        s.y = canvas.height / 2;
        s.z = Math.random() * 0.7 + 0.1;
        s.size = Math.random() * 1.2 + 0.2;
        let angle = Math.random() * Math.PI * 2;
        s.vx = Math.cos(angle);
        s.vy = Math.sin(angle);
    }

    for (let i = 0; i < STAR_COUNT; i++) {
        let s = {};
        resetStar(s);
        stars.push(s);
    }

    function loop() {
        // 1) längere Trails: weniger stark abdunkeln
        ctx.globalCompositeOperation = "source-over";
        ctx.fillStyle = "rgba(0,0,0,0.28)"; // vorher 0.4
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        for (let s of stars) {
            let speed = (s.z * 0.5 + 0.2) * warpFactor;

            let oldX = s.x;
            let oldY = s.y;

            s.x += s.vx * speed * 4;
            s.y += s.vy * speed * 4;
            s.z += 0.015 * warpFactor;

            if (s.x < 0 || s.x > canvas.width || s.y < 0 || s.y > canvas.height) {
                resetStar(s);
                continue;
            }

            const a = 0.35 + s.z * 0.35; // ähnlich wie vorher, nur etwas kontrollierter

            // 2) bläulicher Schimmer (schnell): erst Glow additiv, dann Core normal
            ctx.globalCompositeOperation = "lighter";
            ctx.beginPath();
            ctx.strokeStyle = `rgba(90, 170, 255, ${a * 0.55})`;
            ctx.lineWidth = s.size * 3.2;   // Glow breiter
            ctx.moveTo(oldX, oldY);
            ctx.lineTo(s.x, s.y);
            ctx.stroke();

            ctx.globalCompositeOperation = "source-over";
            ctx.beginPath();
            ctx.strokeStyle = `rgba(220, 245, 255, ${a})`; // etwas "weißlicher" Core
            ctx.lineWidth = s.size;        // Core wie vorher
            ctx.moveTo(oldX, oldY);
            ctx.lineTo(s.x, s.y);
            ctx.stroke();
        }

        requestAnimationFrame(loop);
    }

    window.setWarp = function(factor) {
        warpFactor = factor;
    };

    loop();
}

window.initBackgroundAnim = initStarfield;
