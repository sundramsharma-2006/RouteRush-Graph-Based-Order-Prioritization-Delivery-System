/* RouteRush web - app shell: sidebar, hash router, boot, keyboard shortcuts, global actions. */
(function (root) {
  'use strict';
  const RR = root.RR;
  const $ = (sel, el) => (el || document).querySelector(sel);
  const ROUTES = [
    { id: 'dashboard', label: 'Dashboard', icon: 'dashboard' },
    { id: 'orders', label: 'Orders', icon: 'orders' },
    { id: 'inventory', label: 'Inventory', icon: 'inventory' },
    { id: 'simulate', label: 'Simulation Lab', icon: 'simulate' },
    { id: 'compare', label: 'Compare', icon: 'compare' },
    { id: 'map', label: 'Warehouse Map', icon: 'map' },
    { id: 'about', label: 'How it works', icon: 'about' }
  ];
  let cleanup = null, ready = false;
  const view = $('#view');

  function buildNav() {
    $('#nav').innerHTML = ROUTES.map((r, i) => `<a href="#/${r.id}" data-r="${r.id}" title="${r.label} (${i + 1})">${RR.icon(r.icon, 20)}<span>${r.label}</span><kbd>${i + 1}</kbd></a>`).join('');
  }

  function parseHash() {
    const raw = location.hash.replace(/^#\/?/, '');
    const parts = raw.split('?');
    const params = {};
    if (parts[1]) parts[1].split('&').forEach(p => { const kv = p.split('='); params[decodeURIComponent(kv[0])] = decodeURIComponent(kv[1] || ''); });
    return { route: parts[0] || 'dashboard', params };
  }

  function show() {
    if (!ready) return;
    const { route, params } = parseHash();
    const r = ROUTES.find(x => x.id === route) || ROUTES[0];
    if (cleanup) { try { cleanup(); } catch (e) { console.error(e); } cleanup = null; }
    document.querySelectorAll('#nav a').forEach(a => a.classList.toggle('active', a.dataset.r === r.id));
    document.title = r.label + ' - RouteRush';
    view.classList.remove('page-in'); void view.offsetWidth;
    view.innerHTML = '';
    try { cleanup = RR.pages[r.id](view, params) || null; }
    catch (e) {
      console.error(e);
      view.innerHTML = `<div class="card empty"><h3>This page hit an error</h3><p class="bad">${RR.esc(e.message)}</p><a class="btn primary" href="#/dashboard">Back to dashboard</a></div>`;
    }
    view.classList.add('page-in');
    root.scrollTo(0, 0);
  }

  function status() {
    const st = RR.store.state, el = $('#status');
    if (!st) return;
    el.className = 'status' + (RR.store.demo ? ' demo' : '');
    el.innerHTML = `<i></i>${RR.store.demo ? 'Offline snapshot' : 'Backend online'} &middot; ${st.orders.length} orders &middot; ${st.products.length} products`;
  }

  function offline(msg) {
    view.innerHTML = `<div class="card offline"><h1>Cannot reach the backend</h1><p class="muted">${RR.esc(msg)}</p>
      <p class="muted">Start it with <code>.\\run.bat web</code> (Windows) or <code>make web</code>, then reload this page.</p>
      <button class="btn primary" onclick="location.reload()">Try again</button></div>`;
    const el = $('#status'); el.className = 'status off'; el.innerHTML = '<i></i>Backend offline';
  }

  async function boot() {
    buildNav();
    try {
      await RR.loadState();
    } catch (e) { offline(e.message); return; }
    ready = true;
    RR.log('Connected to the RouteRush backend', 'ok');
    RR.bus.on('state', status);
    status();
    $('#top-new').onclick = () => RR.openOrderModal();
    $('#top-reset').onclick = async () => {
      if (!(await RR.confirm('Reset all data?', 'Orders and stock go back to what was loaded from the CSV files.', 'Reset'))) return;
      try { await RR.api('/api/reset', 'POST'); RR.store.run = null; await RR.loadState(); RR.toast('Data reset', 'ok'); RR.log('Data reset to the CSV files', 'info'); } catch (e) { RR.toast(e.message, 'err'); }
    };
    root.addEventListener('hashchange', show);
    document.addEventListener('keydown', e => {
      if (e.ctrlKey || e.metaKey || e.altKey) return;
      const t = e.target;
      if (t && (t.tagName === 'INPUT' || t.tagName === 'SELECT' || t.tagName === 'TEXTAREA')) return;
      if (document.querySelector('.modal-wrap')) return;
      const n = parseInt(e.key, 10);
      if (n >= 1 && n <= ROUTES.length) location.hash = '#/' + ROUTES[n - 1].id;
      else if (e.key === 'n') RR.openOrderModal();
    });
    show();
  }
  boot();
})(window);
