// Browser glue for TamaBailey. Drives the Wasm core's frame loop, ships
// pixel data from the C++ back-buffer into a 240x240 canvas, and forwards
// keyboard + button taps as input events.

(function () {
  const SCALE = 2;  // display the 240x240 native canvas at 2x for legibility
  const canvas = document.getElementById('screen');
  const ctx    = canvas.getContext('2d');
  // Disable smoothing so pixel art stays crisp when scaled.
  ctx.imageSmoothingEnabled = false;
  canvas.style.imageRendering = 'pixelated';

  // tama::Input enum mapping.
  const INPUT = {
    None: 0, Feed: 1, Play: 2, Clean: 3,
    MenuToggle: 4, PetTap: 5, Restart: 6,
  };

  // We talk to the Wasm side via Module's _functions and HEAPU8.
  // Set up the present hook before the runtime starts.
  window.Module = window.Module || {};

  Module.onRuntimeInitialized = function () {
    // Expose a present hook the C++ code calls via EM_ASM.
    Module.baileyPresent = function (ptr, w, h) {
      const bytes = HEAPU8.subarray(ptr, ptr + w * h * 4);
      // Re-create the ImageData each frame -- copies the bytes.
      const id = new ImageData(new Uint8ClampedArray(bytes), w, h);
      ctx.putImageData(id, 0, 0);
    };

    Module._bailey_init();

    function frame() {
      Module._bailey_frame();
      requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
  };

  function send(code) {
    if (window.Module && Module._bailey_input) Module._bailey_input(code);
  }

  // Buttons
  document.querySelectorAll('[data-input]').forEach(btn => {
    const code = INPUT[btn.dataset.input] ?? 0;
    let longTimer = null;
    function down(ev) {
      ev.preventDefault();
      btn.classList.add('held');
      longTimer = setTimeout(() => {
        send(INPUT.MenuToggle);
        longTimer = null;
      }, 800);
    }
    function up(ev) {
      ev.preventDefault();
      btn.classList.remove('held');
      if (longTimer !== null) {
        clearTimeout(longTimer);
        longTimer = null;
        send(code);
      }
    }
    btn.addEventListener('pointerdown',  down);
    btn.addEventListener('pointerup',    up);
    btn.addEventListener('pointercancel',() => { clearTimeout(longTimer); longTimer = null; btn.classList.remove('held'); });
  });

  // Keyboard: A/B/C = Feed/Play/Clean; M = menu toggle; R = restart;
  // hold any letter for 800ms to act as long-press (menu).
  const keyMap = {
    a: INPUT.Feed, b: INPUT.Play, c: INPUT.Clean,
    m: INPUT.MenuToggle, r: INPUT.Restart,
  };
  let keyTimers = {};
  document.addEventListener('keydown', ev => {
    const k = ev.key.toLowerCase();
    if (!(k in keyMap)) return;
    if (ev.repeat) return;
    ev.preventDefault();
    if (k === 'a' || k === 'b' || k === 'c') {
      keyTimers[k] = setTimeout(() => {
        send(INPUT.MenuToggle);
        keyTimers[k] = null;
      }, 800);
    } else {
      send(keyMap[k]);
    }
  });
  document.addEventListener('keyup', ev => {
    const k = ev.key.toLowerCase();
    if (!(k in keyMap)) return;
    ev.preventDefault();
    if (k === 'a' || k === 'b' || k === 'c') {
      if (keyTimers[k]) { clearTimeout(keyTimers[k]); keyTimers[k] = null; send(keyMap[k]); }
    }
  });

  // Canvas tap = optional touch input. Coordinates map to native 240x240
  // regardless of CSS scaling.
  canvas.addEventListener('click', ev => {
    const rect = canvas.getBoundingClientRect();
    const cx = Math.floor((ev.clientX - rect.left) * canvas.width  / rect.width);
    const cy = Math.floor((ev.clientY - rect.top)  * canvas.height / rect.height);
    if (cy < 32) send(INPUT.MenuToggle);
    else         send(INPUT.PetTap);
  });
})();
