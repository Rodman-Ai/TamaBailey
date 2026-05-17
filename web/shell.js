// Browser glue for TamaBailey. Drives the Wasm core's frame loop, ships
// pixel data from the C++ back-buffer into a 240x240 canvas, forwards
// keyboard + button taps as input events, plays audio via Web Audio,
// and handles sync codes (paste / share).

(function () {
  const canvas = document.getElementById('screen');
  const ctx    = canvas.getContext('2d');
  ctx.imageSmoothingEnabled = false;
  canvas.style.imageRendering = 'pixelated';

  const INPUT = {
    None: 0, Feed: 1, Play: 2, Clean: 3,
    MenuToggle: 4, PetTap: 5, Restart: 6, Stroke: 7, MenuNext: 8,
  };

  // ---- Web Audio ----
  let audioCtx = null;
  function ensureAudio() {
    if (audioCtx) return audioCtx;
    try { audioCtx = new (window.AudioContext || window.webkitAudioContext)(); }
    catch (e) { return null; }
    return audioCtx;
  }
  function playPcm(ptr, n, sr, vol) {
    const ac = ensureAudio();
    if (!ac) return;
    const int16 = new Int16Array(Module.HEAP16.buffer, ptr, n);
    const buf = ac.createBuffer(1, n, sr);
    const ch = buf.getChannelData(0);
    for (let i = 0; i < n; ++i) ch[i] = int16[i] / 32768;
    const src = ac.createBufferSource();
    src.buffer = buf;
    const gain = ac.createGain();
    gain.gain.value = Math.max(0, Math.min(1, vol / 100));
    src.connect(gain).connect(ac.destination);
    src.start();
  }

  window.Module = window.Module || {};

  Module.onRuntimeInitialized = function () {
    Module.baileyPresent = function (ptr, w, h) {
      const bytes = Module.HEAPU8.subarray(ptr, ptr + w * h * 4);
      const id = new ImageData(new Uint8ClampedArray(bytes), w, h);
      ctx.putImageData(id, 0, 0);
    };
    Module.baileyPlay = playPcm;

    Module._bailey_init();

    // Handle ?bailey=<sync_code> spectator URLs.
    const params = new URLSearchParams(window.location.search);
    if (params.has('bailey')) {
      const code = params.get('bailey');
      const ok = Module.ccall('bailey_apply_sync_code', 'number', ['string'], [code]);
      if (ok) {
        Module.ccall('bailey_set_spectator', null, ['number'], [1]);
        document.getElementById('spectatorBanner')?.classList.add('show');
      }
    }

    function frame() {
      Module._bailey_frame();
      requestAnimationFrame(frame);
    }
    requestAnimationFrame(frame);
  };

  function send(code) {
    if (window.Module && Module._bailey_input) Module._bailey_input(code);
  }

  // ---- Main 3 buttons (Feed/Play/Clean) with long-press = menu ----
  document.querySelectorAll('.btn[data-input]').forEach(btn => {
    const code = INPUT[btn.dataset.input] ?? 0;
    let longTimer = null;
    function down(ev) {
      ev.preventDefault();
      ensureAudio();
      btn.classList.add('held');
      longTimer = setTimeout(() => { send(INPUT.MenuToggle); longTimer = null; }, 800);
    }
    function up(ev) {
      ev.preventDefault();
      btn.classList.remove('held');
      if (longTimer !== null) { clearTimeout(longTimer); longTimer = null; send(code); }
    }
    btn.addEventListener('pointerdown',  down);
    btn.addEventListener('pointerup',    up);
    btn.addEventListener('pointercancel', () => { clearTimeout(longTimer); longTimer = null; btn.classList.remove('held'); });
  });

  // ---- Cosmetic / utility buttons (plain clicks) ----
  document.querySelectorAll('.link-btn[data-input]').forEach(btn => {
    const code = INPUT[btn.dataset.input] ?? 0;
    btn.addEventListener('click', ev => {
      ev.preventDefault();
      ensureAudio();
      send(code);
      // The Take-photo button additionally downloads the canvas PNG.
      if (btn.id === 'photoBtn') {
        // Wait one frame so the screen reflects current state.
        requestAnimationFrame(() => {
          const a = document.createElement('a');
          a.download = 'bailey-' + Date.now() + '.png';
          a.href = canvas.toDataURL('image/png');
          a.click();
        });
      }
    });
  });

  // ---- Keyboard ----
  const keyMap = {
    a: INPUT.Feed, b: INPUT.Play, c: INPUT.Clean,
    m: INPUT.MenuToggle, r: INPUT.Restart, n: INPUT.MenuNext,
  };
  let keyTimers = {};
  document.addEventListener('keydown', ev => {
    const k = ev.key.toLowerCase();
    if (!(k in keyMap)) return;
    if (ev.repeat) return;
    if (k === 'a' || k === 'b' || k === 'c') {
      ev.preventDefault();
      keyTimers[k] = setTimeout(() => { send(INPUT.MenuToggle); keyTimers[k] = null; }, 800);
    } else {
      ev.preventDefault();
      send(keyMap[k]);
    }
  });
  document.addEventListener('keyup', ev => {
    const k = ev.key.toLowerCase();
    if (!(k in keyMap)) return;
    if (k === 'a' || k === 'b' || k === 'c') {
      ev.preventDefault();
      if (keyTimers[k]) { clearTimeout(keyTimers[k]); keyTimers[k] = null; send(keyMap[k]); }
    }
  });

  // ---- Canvas touch / drag ----
  let dragging = false;
  function nativePos(ev) {
    const rect = canvas.getBoundingClientRect();
    return {
      x: Math.floor((ev.clientX - rect.left) * canvas.width  / rect.width),
      y: Math.floor((ev.clientY - rect.top)  * canvas.height / rect.height),
    };
  }
  canvas.addEventListener('pointerdown', ev => {
    ensureAudio();
    const p = nativePos(ev);
    if (p.y < 32)  { send(INPUT.MenuToggle); return; }
    send(INPUT.PetTap);
    dragging = true;
    canvas.setPointerCapture(ev.pointerId);
  });
  canvas.addEventListener('pointermove', ev => {
    if (!dragging) return;
    send(INPUT.Stroke);
  });
  canvas.addEventListener('pointerup', ev => {
    dragging = false;
    try { canvas.releasePointerCapture(ev.pointerId); } catch (_) {}
  });

  // ---- Sync code share/paste UI ----
  window.addEventListener('load', () => {
    const shareBtn = document.getElementById('shareBtn');
    const pasteBtn = document.getElementById('pasteBtn');
    if (shareBtn) shareBtn.addEventListener('click', () => {
      if (!Module._bailey_generate_sync_code) return;
      const cstr = Module.ccall('bailey_generate_sync_code', 'string', [], []);
      const url = window.location.origin + window.location.pathname + '?bailey=' + cstr.replace(/-/g, '');
      navigator.clipboard?.writeText(url).then(() => {
        shareBtn.textContent = 'Copied!';
        setTimeout(() => shareBtn.textContent = 'Share Bailey', 1500);
      });
    });
    if (pasteBtn) pasteBtn.addEventListener('click', () => {
      const code = prompt("Paste your Bailey sync code:");
      if (!code) return;
      const ok = Module.ccall('bailey_apply_sync_code', 'number', ['string'], [code]);
      alert(ok ? "Bailey restored!" : "That code doesn't look right -- try again?");
    });
  });
})();
