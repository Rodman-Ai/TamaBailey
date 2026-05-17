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
    CycleScene: 9, CycleCoat: 10, CycleAccessory: 11,
    TakePhoto: 12, MicTrigger: 13,
    Walk: 14, TreatGive: 15, Brush: 16, CycleToy: 17,
    CycleAge: 18, ImuFlick: 19,
    VoiceSit: 20, VoiceCome: 21, VoiceHighFive: 22,
    VoiceRollOver: 23, VoiceJump: 24,
    Bedtime: 25, MenuCursorNext: 26,
    PlayWithFriend: 27,
    PlayWithFriendOllie: 28, PlayWithFriendMitchell: 29,
    PlayWithFriendEnzo: 30, PlayWithFriendLincoln: 31,
    PlayWithFriendRuben: 32,
    PlayWithFriendFrancie: 33, PlayWithFriendBomi: 34, PlayWithFriendNoshy: 35,
    ImuShake: 36,
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
    // Handle ?bio=1 -- render a downloadable bio card and offer download.
    if (params.has('bio')) {
      // Wait two frames so the game has rendered Bailey's current pose.
      requestAnimationFrame(() => requestAnimationFrame(renderBioCard));
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
    w: INPUT.Walk,       // start a walk / advance a step (motion-control proxy)
    s: INPUT.ImuShake,   // shake gesture proxy
    f: INPUT.ImuFlick,   // forward flick proxy (advances fetch)
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

  // ---- Real-world weather sync (best-effort via wttr.in, no key) ----
  // Maps wttr.in condition codes to our Weather enum:
  //   0 sunny, 1 cloudy, 2 rain, 3 snow.
  async function syncRealWeather() {
    try {
      const res = await fetch("https://wttr.in/?format=j1", { mode: 'cors' });
      if (!res.ok) return;
      const data = await res.json();
      const code = +(data.current_condition?.[0]?.weatherCode || 113);
      let w = 0;
      if ([113].includes(code))                                  w = 0;       // clear/sunny
      else if ([116, 119, 122, 143, 248, 260].includes(code))    w = 1;       // cloudy/fog
      else if (code >= 176 && code <= 311 && ![179,182,185,227,230].includes(code)) w = 2; // rain
      else if ([179,182,185,227,230,323,326,329,332,335,338,350,
                 368,371,374,377,392,395].includes(code))         w = 3;       // snow
      else                                                        w = 1;
      if (Module._bailey_set_weather) Module._bailey_set_weather(w);
    } catch (e) {
      // Silent: keep the synthetic daily roll.
    }
  }

  // ---- Browser notifications (opt-in, low-stat reminders) ----
  let notifEnabled = false;
  let lastNotifAt = 0;
  function maybeNotify() {
    if (!notifEnabled) return;
    if (document.visibilityState === 'visible') return;
    const now = Date.now();
    if (now - lastNotifAt < 10 * 60 * 1000) return;        // at most every 10 min
    if (!('Notification' in window) || Notification.permission !== 'granted') return;
    if (!Module._bailey_get_stat) return;
    const stats = [0,1,2,3].map(i => Module._bailey_get_stat(i));
    const min = Math.min(...stats);
    if (min >= 20) return;
    const labels = ['food', 'play', 'bath', 'rest'];
    const idx = stats.indexOf(min);
    new Notification("Bailey needs " + labels[idx], {
      body: "Stats are low (" + min + "/100). Check on him?",
      icon: undefined,
      tag: 'tama-bailey-low-' + idx,
    });
    lastNotifAt = now;
  }

  // ---- Microphone reactions (optional, user must grant permission) ----
  let micEnabled = false;
  let lastMicTrigger = 0;
  async function startMic() {
    if (micEnabled) return;
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      const ac = ensureAudio(); if (!ac) return;
      const src = ac.createMediaStreamSource(stream);
      const analyzer = ac.createAnalyser();
      analyzer.fftSize = 512;
      src.connect(analyzer);
      const data = new Uint8Array(analyzer.frequencyBinCount);
      micEnabled = true;
      function tick() {
        if (!micEnabled) return;
        analyzer.getByteTimeDomainData(data);
        let max = 0;
        for (let i = 0; i < data.length; ++i) {
          const v = Math.abs(data[i] - 128);
          if (v > max) max = v;
        }
        const now = performance.now();
        if (max > 60 && now - lastMicTrigger > 1500) {
          send(INPUT.MicTrigger);
          lastMicTrigger = now;
        }
        requestAnimationFrame(tick);
      }
      tick();
    } catch (e) {
      console.warn("Mic access denied:", e);
      micEnabled = false;
    }
  }

  window.addEventListener('load', () => {
    const micBtn = document.getElementById('micBtn');
    if (micBtn) micBtn.addEventListener('click', () => {
      startMic();
      micBtn.textContent = micEnabled ? 'Mic on' : 'Enable mic';
    });
    const notifBtn = document.getElementById('notifBtn');
    if (notifBtn) notifBtn.addEventListener('click', async () => {
      if (!('Notification' in window)) {
        notifBtn.textContent = 'Notifs unsupported';
        return;
      }
      const p = await Notification.requestPermission();
      notifEnabled = (p === 'granted');
      notifBtn.textContent = notifEnabled ? 'Reminders on' : 'Enable reminders';
    });
    // Kick off the weather sync once the Wasm runtime is ready.
    const tryWeather = () => {
      if (Module && Module._bailey_set_weather) {
        syncRealWeather();
        // Re-sync once an hour.
        setInterval(syncRealWeather, 60 * 60 * 1000);
      } else {
        setTimeout(tryWeather, 500);
      }
    };
    tryWeather();
    setInterval(maybeNotify, 60 * 1000);  // check once a minute
  });

  // ---- Bio card renderer (?bio=1) ----
  function renderBioCard() {
    const stats = [0,1,2,3].map(i => Module._bailey_get_stat ? Module._bailey_get_stat(i) : 0);
    const off = document.createElement('canvas');
    off.width = 480; off.height = 720;
    const o = off.getContext('2d');
    // Background
    o.fillStyle = '#1f2227'; o.fillRect(0, 0, off.width, off.height);
    // Title
    o.fillStyle = '#f0c060';
    o.font = 'bold 36px ui-sans-serif, system-ui';
    o.textAlign = 'center';
    o.fillText('BAILEY', off.width/2, 56);
    o.fillStyle = '#8a8f96';
    o.font = '16px ui-sans-serif, system-ui';
    o.fillText('a hound on the internet', off.width/2, 80);
    // Bailey's current screen as a 240x240 inset, 2x scaled to 480x480
    o.imageSmoothingEnabled = false;
    o.drawImage(canvas, 0, 0, 240, 240, 0, 100, 480, 480);
    // Footer stats
    o.fillStyle = '#e8ebee';
    o.font = '18px ui-monospace, monospace';
    o.textAlign = 'left';
    const labels = ['Food', 'Play', 'Bath', 'Rest'];
    for (let i = 0; i < 4; ++i) {
      o.fillText(labels[i] + ': ' + stats[i] + '/100', 30 + i*110, 610);
    }
    o.fillStyle = '#f0c060';
    o.font = '12px ui-sans-serif, system-ui';
    o.fillText('https://rodman-ai.github.io/TamaBailey/', 30, 700);
    // Download
    const link = document.createElement('a');
    link.download = 'bailey-bio-' + Date.now() + '.png';
    link.href = off.toDataURL('image/png');
    link.textContent = 'Click to download Bailey’s bio card';
    link.className = 'link-btn';
    link.style.fontSize = '16px';
    link.style.display = 'block';
    link.style.margin = '20px auto';
    document.querySelector('main')?.prepend(link);
  }

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
