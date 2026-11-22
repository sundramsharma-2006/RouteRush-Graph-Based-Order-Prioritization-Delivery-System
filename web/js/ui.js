/* RouteRush web - UI helpers: toasts, modals, animated counters, 3D tilt, ripple, confetti, SVG charts. */
(function (root) {
  'use strict';
  const RR = root.RR = root.RR || {};
  const reduced = () => root.matchMedia && root.matchMedia('(prefers-reduced-motion: reduce)').matches;
  RR.reducedMotion = reduced;

  RR.$ = (sel, el) => (el || document).querySelector(sel);
  RR.$$ = (sel, el) => Array.from((el || document).querySelectorAll(sel));
  RR.h = html => { const t = document.createElement('template'); t.innerHTML = html.trim(); return t.content.firstElementChild; };

  // ---------------------------------------------------------------- toasts
  RR.toast = function (msg, kind) {
    const box = document.getElementById('toasts');
    if (!box) return;
    const el = RR.h(`<div class="toast ${kind || 'info'}"><span class="dot"></span><span>${RR.esc(msg)}</span></div>`);
    box.appendChild(el);
    requestAnimationFrame(() => el.classList.add('show'));
    setTimeout(() => { el.classList.remove('show'); setTimeout(() => el.remove(), 400); }, kind === 'err' ? 6000 : 3200);
  };

  // ---------------------------------------------------------------- modal
  RR.modal = function (opt) {
    const rootEl = document.getElementById('modal-root');
    const wrap = RR.h(`<div class="modal-wrap"><div class="modal" role="dialog" aria-modal="true" aria-label="${RR.esc(opt.title)}">
        <div class="modal-head"><h3>${RR.esc(opt.title)}</h3><button class="icon-btn" data-close aria-label="Close">&times;</button></div>
        <div class="modal-body">${opt.body}</div></div></div>`);
    rootEl.appendChild(wrap);
    requestAnimationFrame(() => wrap.classList.add('open'));
    const api = {
      el: wrap.querySelector('.modal'),
      close() {
        document.removeEventListener('keydown', onKey);
        wrap.classList.remove('open');
        setTimeout(() => wrap.remove(), 250);
        if (opt.onClose) opt.onClose();
      }
    };
    const onKey = e => { if (e.key === 'Escape') api.close(); };
    document.addEventListener('keydown', onKey);
    wrap.addEventListener('mousedown', e => { if (e.target === wrap) api.close(); });
    wrap.querySelector('[data-close]').onclick = api.close;
    if (opt.onMount) opt.onMount(api.el, api);
    const first = wrap.querySelector('input,select,button.primary');
    if (first) setTimeout(() => first.focus(), 60);
    return api;
  };

  RR.confirm = function (title, text, okLabel) {
    return new Promise(resolve => {
      let answered = false;
      const m = RR.modal({
        title,
        body: `<p class="muted">${RR.esc(text)}</p><div class="row end"><button class="btn ghost" data-no>Cancel</button><button class="btn danger" data-yes>${RR.esc(okLabel || 'Confirm')}</button></div>`,
        onClose() { if (!answered) resolve(false); },
        onMount(el, api) {
          el.querySelector('[data-no]').onclick = () => api.close();
          el.querySelector('[data-yes]').onclick = () => { answered = true; resolve(true); api.close(); };
        }
      });
      return m;
    });
  };

  // ---------------------------------------------------------------- animated number
  RR.countUp = function (el, to, opt) {
    opt = opt || {};
    const dec = opt.decimals || 0, dur = opt.duration || 900;
    if (to === null || to === undefined || Number.isNaN(to)) { el.textContent = '-'; return; }
    if (reduced()) { el.textContent = to.toFixed(dec); return; }
    const from = parseFloat(el.dataset.v || '0') || 0;
    el.dataset.v = String(to);
    const t0 = performance.now();
    (function step(now) {
      const t = Math.min(1, (now - t0) / dur);
      el.textContent = (from + (to - from) * RR.ease(t)).toFixed(dec);
      if (t < 1 && el.isConnected) requestAnimationFrame(step);
    })(t0);
  };

  // ---------------------------------------------------------------- 3D tilt on .tilt elements (delegated)
  let tiltEl = null;
  document.addEventListener('pointermove', e => {
    if (reduced() || e.pointerType === 'touch') return;
    const el = e.target.closest ? e.target.closest('.tilt') : null;
    if (tiltEl && tiltEl !== el) { tiltEl.style.transform = ''; tiltEl.style.setProperty('--gx', '50%'); tiltEl.style.setProperty('--gy', '0%'); }
    tiltEl = el;
    if (!el) return;
    const r = el.getBoundingClientRect();
    const px = (e.clientX - r.left) / r.width, py = (e.clientY - r.top) / r.height;
    el.style.transform = `perspective(900px) rotateX(${((0.5 - py) * 7).toFixed(2)}deg) rotateY(${((px - 0.5) * 9).toFixed(2)}deg) translateZ(0)`;
    el.style.setProperty('--gx', (px * 100).toFixed(0) + '%');
    el.style.setProperty('--gy', (py * 100).toFixed(0) + '%');
  });
  document.addEventListener('pointerleave', () => { if (tiltEl) { tiltEl.style.transform = ''; tiltEl = null; } }, true);

  // ---------------------------------------------------------------- button ripple (delegated)
  document.addEventListener('pointerdown', e => {
    const b = e.target.closest ? e.target.closest('.btn') : null;
    if (!b || reduced()) return;
    const r = b.getBoundingClientRect();
    const s = document.createElement('span');
    s.className = 'ripple';
    s.style.left = (e.clientX - r.left) + 'px';
    s.style.top = (e.clientY - r.top) + 'px';
    b.appendChild(s);
    setTimeout(() => s.remove(), 650);
  });

  // ---------------------------------------------------------------- confetti burst (canvas overlay)
  RR.confetti = function (x, y, count) {
    if (reduced()) return;
    const c = document.createElement('canvas');
    c.className = 'confetti';
    c.width = root.innerWidth; c.height = root.innerHeight;
    document.body.appendChild(c);
    const ctx = c.getContext('2d');
    const colors = ['#e0793f', '#e8b04a', '#d4483b', '#c8645a', '#8fae6b'];
    const ps = [];
    for (let i = 0; i < (count || 70); i++) {
      const a = Math.random() * Math.PI * 2, v = 3 + Math.random() * 7;
      ps.push({ x, y, vx: Math.cos(a) * v, vy: Math.sin(a) * v - 4, s: 3 + Math.random() * 4, c: colors[i % colors.length], r: Math.random() * 6, vr: (Math.random() - 0.5) * 0.4, life: 1 });
    }
    (function frame() {
      ctx.clearRect(0, 0, c.width, c.height);
      let alive = false;
      ps.forEach(p => {
        p.vy += 0.28; p.vx *= 0.99; p.x += p.vx; p.y += p.vy; p.r += p.vr; p.life -= 0.012;
        if (p.life > 0) {
          alive = true;
          ctx.save(); ctx.globalAlpha = Math.max(0, p.life); ctx.translate(p.x, p.y); ctx.rotate(p.r);
          ctx.fillStyle = p.c; ctx.fillRect(-p.s, -p.s / 2, p.s * 2, p.s); ctx.restore();
        }
      });
      if (alive) requestAnimationFrame(frame); else c.remove();
    })();
  };

  // ---------------------------------------------------------------- SVG donut chart
  // parts: [{label, value, color}]. Returns an SVG string; the arcs sweep in through CSS.
  RR.donut = function (parts, size, label) {
    size = size || 150;
    const total = parts.reduce((s, p) => s + p.value, 0);
    const r = size / 2 - 14, c = 2 * Math.PI * r;
    let off = 0, arcs = '';
    if (total > 0) {
      parts.forEach((p, i) => {
        const len = (p.value / total) * c;
        if (len <= 0) return;
        arcs += `<circle class="arc" cx="${size / 2}" cy="${size / 2}" r="${r}" fill="none" stroke="${p.color}" stroke-width="14" stroke-linecap="butt"
          stroke-dasharray="${Math.max(0, len - 3).toFixed(2)} ${(c - Math.max(0, len - 3)).toFixed(2)}" stroke-dashoffset="${(-off).toFixed(2)}"
          transform="rotate(-90 ${size / 2} ${size / 2})" style="animation-delay:${i * 120}ms"/>`;
        off += len;
      });
    }
    return `<svg class="donut" viewBox="0 0 ${size} ${size}" width="${size}" height="${size}" role="img" aria-label="${RR.esc(label || 'chart')}">
      <circle cx="${size / 2}" cy="${size / 2}" r="${r}" fill="none" stroke="rgba(255,255,255,.07)" stroke-width="14"/>${arcs}
      <text x="50%" y="48%" text-anchor="middle" class="donut-n">${total}</text><text x="50%" y="62%" text-anchor="middle" class="donut-l">${RR.esc(label || '')}</text></svg>`;
  };

  RR.nowTime = d => d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
})(typeof window !== 'undefined' ? window : globalThis);
