/* RouteRush web - the seven pages. Each page is a function (view) -> cleanup(); it renders into #view,
   subscribes to state changes, and releases the 3D scene / timers when you leave. */
(function (root) {
  'use strict';
  const RR = root.RR;
  const { esc } = RR;
  const $ = (sel, el) => (el || document).querySelector(sel);
  const $$ = (sel, el) => Array.from((el || document).querySelectorAll(sel));
  const S = () => RR.store.state;
  RR.pages = {};
  RR.pending = {};                                   // one-shot hints between pages, e.g. { run: true }
  const TICKS_PER_SECOND = 14;                       // playback speed at 1x

  // ------------------------------------------------------------------ icons (inline SVG, stroke = currentColor)
  const ICONS = {
    dashboard: '<path d="M3 13h8V3H3zM13 21h8V11h-8zM3 21h8v-6H3zM13 3v6h8V3z"/>',
    orders: '<path d="M6 2h12v20l-3-2-3 2-3-2-3 2z"/><path d="M9 7h6M9 11h6M9 15h4"/>',
    inventory: '<path d="M21 8l-9-5-9 5 9 5z"/><path d="M3 8v8l9 5 9-5V8M12 13v8"/>',
    simulate: '<circle cx="12" cy="12" r="9"/><path d="M10 8l6 4-6 4z"/>',
    compare: '<path d="M5 21V9M12 21V3M19 21v-8"/>',
    map: '<circle cx="6" cy="6" r="2.5"/><circle cx="18" cy="8" r="2.5"/><circle cx="9" cy="18" r="2.5"/><path d="M8 7l8 1M7 8l2 8M17 10l-6 7"/>',
    about: '<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7.5v.5"/>',
    play: '<path d="M7 4l13 8-13 8z"/>', pause: '<path d="M7 4h4v16H7zM13 4h4v16h-4z"/>',
    restart: '<path d="M3 12a9 9 0 1 0 3-6.7M3 4v5h5"/>', plus: '<path d="M12 5v14M5 12h14"/>',
    trash: '<path d="M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13"/>', bolt: '<path d="M13 2L4 14h7l-1 8 9-12h-7z"/>',
    reset: '<path d="M3 12a9 9 0 1 0 3-6.7M3 4v5h5"/>', box: '<path d="M21 8l-9-5-9 5 9 5z"/><path d="M3 8v8l9 5 9-5V8"/>'
  };
  RR.icon = (name, size) => `<svg class="ico" width="${size || 18}" height="${size || 18}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${ICONS[name] || ''}</svg>`;

  const chip = p => `<span class="chip p-${esc(p)}">${esc(p)}</span>`;
  const nodeName = id => { const n = S().nodes[id]; return n ? n.name : '?'; };
  const kpi = (id, label, sub, accent) => `<div class="card kpi tilt" style="--accent:${accent}"><div class="kpi-label">${label}</div><div class="kpi-val" id="${id}">0</div><div class="kpi-sub" id="${id}-s">${sub || ''}</div></div>`;
  const fallbackBox = '<div class="scene-fallback"><b>3D view unavailable</b><p>It needs WebGL and the three.js library. Connect to the internet once, or save <code>three.min.js</code> into <code>web/vendor/</code>, then reload.</p></div>';

  function mountScene(box, opts) {
    if (!RR.hasWebGL()) { box.innerHTML = fallbackBox; return null; }
    try {
      const sc = new RR.WarehouseScene(box, opts);
      sc.setGraph(RR.sceneGraph(S()));
      return sc;
    } catch (e) { console.error(e); box.innerHTML = fallbackBox; return null; }
  }
  function setText(el, v) { if (el) el.textContent = v; }

  // ------------------------------------------------------------------ shared: new-order dialog
  RR.openOrderModal = function () {
    const st = S();
    const dests = st.nodes.filter(n => RR.nodeType(st, n.id) === 'city');
    const dflt = dests.length ? dests[dests.length - 1].id : (st.nodes[0] ? st.nodes[0].id : 0);
    const prioInfo = { URGENT: 'Served first. Short waits, may delay others.', EXPRESS: 'Between urgent and normal.', NORMAL: 'Served by aging when no one more important waits.' };
    const prods = st.products.map(p => `<option value="${esc(p.id)}">${esc(p.id)} - ${esc(p.name)}</option>`).join('');
    const body = `
      <form id="of" class="form" autocomplete="off">
        <label class="field">Customer<input id="of-cust" class="input" maxlength="30" placeholder="e.g. Zara" required></label>
        <div class="field">Deliver to
          <div class="seg wrap" id="of-dest">${dests.map(n => `<button type="button" class="${n.id === dflt ? 'on' : ''}" data-v="${n.id}">${esc(n.name)}<small>${RR.fmt1(n.dist)} away</small></button>`).join('')}</div></div>
        <div class="field">Priority
          <div class="seg prio" id="of-prio">${RR.PRIORITIES.slice().reverse().map((p, i) => `<button type="button" class="p-${p} ${i === 2 ? 'on' : ''}" data-v="${p}">${p}</button>`).join('')}</div>
          <small class="muted" id="of-prio-t">${prioInfo.NORMAL}</small></div>
        <div class="row">
          <label class="field">Arrival tick<input id="of-arr" class="input" type="number" min="0" value="0"></label>
          <label class="field">Deadline tick<input id="of-dead" class="input" type="number" min="0" placeholder="none"></label>
        </div>
        <div class="field">Items <div id="of-lines"></div><button type="button" class="btn sm" id="of-add">${RR.icon('plus', 14)} Add item</button></div>
        <div class="form-error" id="of-err" role="alert"></div>
        <div class="row end"><button type="button" class="btn ghost" data-close2>Cancel</button><button class="btn primary" type="submit">${RR.icon('bolt', 16)} Place order</button></div>
      </form>`;
    RR.modal({
      title: 'New order', body,
      onMount(el, api) {
        let dest = String(dflt), prio = 'NORMAL';
        const err = $('#of-err', el);
        const segPick = (id, cb) => $('#' + id, el).addEventListener('click', e => {
          const b = e.target.closest('button[data-v]'); if (!b) return;
          $$('button', b.parentElement).forEach(x => x.classList.remove('on')); b.classList.add('on'); cb(b.dataset.v);
        });
        segPick('of-dest', v => { dest = v; });
        segPick('of-prio', v => { prio = v; $('#of-prio-t', el).textContent = prioInfo[v]; });
        const demand = RR.demand(S());
        const hint = row => {
          const pid = $('select', row).value, q = parseInt($('input', row).value || '0', 10);
          const p = S().products.find(x => x.id === pid);
          const asked = (demand[pid] || 0);
          const el2 = $('.hint', row);
          if (!p) { el2.textContent = ''; return; }
          el2.textContent = `${p.qty} in stock, ${asked} already requested`;
          el2.className = 'hint ' + (q + asked > p.qty ? 'warn' : '');
        };
        const addLine = () => {
          const row = RR.h(`<div class="line"><select class="input">${prods}</select>
            <div class="stepper"><button type="button" data-d="-1" aria-label="less">-</button><input class="input" type="number" min="1" value="1"><button type="button" data-d="1" aria-label="more">+</button></div>
            <span class="hint"></span><button type="button" class="icon-btn" aria-label="Remove item">&times;</button></div>`);
          $('#of-lines', el).appendChild(row);
          row.addEventListener('input', () => hint(row)); row.addEventListener('change', () => hint(row));
          row.addEventListener('click', e => {
            const d = e.target.closest('[data-d]');
            if (d) { const i = $('input', row); i.value = Math.max(1, (parseInt(i.value || '1', 10) || 1) + parseInt(d.dataset.d, 10)); hint(row); }
            if (e.target.closest('.icon-btn') && $$('.line', el).length > 1) row.remove();
          });
          hint(row);
        };
        addLine();
        $('#of-add', el).onclick = addLine;
        $('[data-close2]', el).onclick = api.close;
        $('#of', el).onsubmit = async ev => {
          ev.preventDefault(); err.textContent = '';
          const lines = $$('.line', el).map(l => $('select', l).value + ':' + $('input', l).value).join(';');
          const btn = $('button[type=submit]', el); btn.disabled = true;
          try {
            const r = await RR.api('/api/order', 'POST', { customer: $('#of-cust', el).value, dest, priority: prio, arrival: $('#of-arr', el).value, deadline: $('#of-dead', el).value, lines });
            const rect = btn.getBoundingClientRect();
            RR.confetti(rect.left + rect.width / 2, rect.top, 80);
            RR.toast(`Order ${r.id} placed`, 'ok'); RR.log(`Order ${r.id} placed (${prio}, ${nodeName(parseInt(dest, 10))})`, 'ok');
            await RR.loadState(); api.close();
          } catch (e) { err.textContent = e.message; btn.disabled = false; }
        };
      }
    });
  };

  // ================================================================== DASHBOARD
  RR.pages.dashboard = function (view) {
    view.innerHTML = `
      <div class="page-head"><div><h1>Command Center</h1><p class="muted">Orders, stock and routes at a glance. Everything below is computed by the C++ backend.</p></div>
        <div class="row"><button class="btn primary" id="d-run">${RR.icon('play', 16)} Run simulation</button><button class="btn" id="d-new">${RR.icon('plus', 16)} New order</button></div></div>
      <div class="grid kpis">
        ${kpi('k-orders', 'Orders waiting', '', 'var(--cyan)')}${kpi('k-prods', 'Products', '', 'var(--yellow)')}
        ${kpi('k-deliv', 'Delivered (last run)', '', 'var(--green)')}${kpi('k-wait', 'URGENT avg wait', '', 'var(--pink)')}
      </div>
      <div class="grid dash-main">
        <div class="card scene-card"><div class="card-head"><h2>Warehouse map</h2><a class="btn sm ghost" href="#/map">Open full map</a></div>
          <div class="scene-box" id="d-scene"></div>
          <div class="legend"><span><i style="background:#f1e6d8"></i>Depot</span><span><i style="background:#8fae6b"></i>Shelves</span><span><i style="background:#e8b04a"></i>Packing</span><span><i style="background:#c8645a"></i>Customers</span><span class="muted">Drag to orbit, scroll to zoom, click a node.</span></div></div>
        <div class="col">
          <div class="card"><h2>Priority mix</h2><div class="donut-wrap" id="d-donut"></div><div class="legend" id="d-donut-l"></div></div>
          <div class="card"><h2>Live activity</h2><ul class="feed" id="d-feed"></ul></div>
        </div>
      </div>
      <div class="grid cols-2">
        <div class="card"><div class="card-head"><h2>Stock vs demand</h2><a class="btn sm ghost" href="#/inventory">Inventory</a></div><div id="d-stock"></div></div>
        <div class="card"><div class="card-head"><h2>Last run: waiting by priority</h2><a class="btn sm ghost" href="#/compare">Compare</a></div><div id="d-wait"></div></div>
      </div>`;
    const scene = mountScene($('#d-scene', view), { autoRotate: true, labels: true, pulses: true, onSelect: id => { if (id !== null) location.hash = '#/map?node=' + id; } });

    function renderFeed() {
      $('#d-feed', view).innerHTML = RR.store.feed.slice(0, 7).map(f => `<li class="${f.kind}"><time>${RR.nowTime(f.at)}</time><span>${esc(f.text)}</span></li>`).join('') || '<li class="muted">Nothing yet.</li>';
    }
    function refresh() {
      const st = S(), run = RR.store.run;
      RR.countUp($('#k-orders', view), st.orders.length);
      setText($('#k-orders-s', view), `${st.orders.filter(o => o.priority === 'URGENT').length} urgent, ${st.orders.filter(o => o.priority === 'EXPRESS').length} express`);
      RR.countUp($('#k-prods', view), st.products.length);
      setText($('#k-prods-s', view), `${st.products.reduce((s, p) => s + p.qty, 0)} units on the shelves`);
      if (run) {
        RR.countUp($('#k-deliv', view), run.metrics.delivered);
        setText($('#k-deliv-s', view), `${run.metrics.rejected} rejected, ${run.metrics.late} late`);
        RR.countUp($('#k-wait', view), run.metrics.wait.URGENT.avg, { decimals: 1 });
        setText($('#k-wait-s', view), `ticks (${run.policy.fifo ? 'FIFO' : 'priority + aging'})`);
      } else {
        setText($('#k-deliv', view), '-'); setText($('#k-deliv-s', view), 'run a simulation');
        setText($('#k-wait', view), '-'); setText($('#k-wait-s', view), 'run a simulation');
      }
      const mix = RR.PRIORITIES.map(p => ({ label: p, value: st.orders.filter(o => o.priority === p).length, color: RR.PRIO_COLOR[p] }));
      $('#d-donut', view).innerHTML = RR.donut(mix, 150, 'orders');
      $('#d-donut-l', view).innerHTML = mix.map(m => `<span><i style="background:${m.color}"></i>${m.label} ${m.value}</span>`).join('');
      const demand = RR.demand(st);
      $('#d-stock', view).innerHTML = st.products.map(p => {
        const d = demand[p.id] || 0, mx = Math.max(p.qty, d, 1);
        return `<div class="srow"><div class="srow-h"><b>${esc(p.name)}</b><span class="${d > p.qty ? 'bad' : 'muted'}">${p.qty} in stock, ${d} requested${d > p.qty ? ' (short ' + (d - p.qty) + ')' : ''}</span></div>
          <div class="meter"><i class="${d > p.qty ? 'low' : ''}" style="width:${(p.qty / mx * 100).toFixed(1)}%"></i><u style="left:${(d / mx * 100).toFixed(1)}%"></u></div></div>`;
      }).join('') || '<p class="muted">No products.</p>';
      if (run) {
        const mx = Math.max(1, ...RR.PRIORITIES.map(p => run.metrics.wait[p].avg));
        $('#d-wait', view).innerHTML = RR.PRIORITIES.map(p => `<div class="hbar"><span class="${'t-' + p}">${p}</span><div class="meter"><i style="width:${(run.metrics.wait[p].avg / mx * 100).toFixed(1)}%;background:${RR.PRIO_COLOR[p]}"></i></div><b>${RR.fmt1(run.metrics.wait[p].avg)}</b></div>`).join('');
      } else $('#d-wait', view).innerHTML = '<p class="muted">Run a simulation to see how long each priority waits.</p>';
      if (scene) scene.setGraph(RR.sceneGraph(st));
      renderFeed();
    }
    $('#d-run', view).onclick = () => { RR.pending.run = true; location.hash = '#/simulate'; };
    $('#d-new', view).onclick = () => RR.openOrderModal();
    const offs = [RR.bus.on('state', refresh), RR.bus.on('feed', renderFeed)];
    refresh();
    return () => { offs.forEach(f => f()); if (scene) scene.destroy(); };
  };

  // ================================================================== ORDERS
  RR.pages.orders = function (view) {
    let filter = 'ALL', q = '';
    view.innerHTML = `
      <div class="page-head"><div><h1>Orders</h1><p class="muted">Everything waiting to be processed. The simulator replays this list.</p></div>
        <div class="row"><button class="btn ghost" id="o-reset">${RR.icon('reset', 16)} Reset data</button><button class="btn primary" id="o-new">${RR.icon('plus', 16)} New order</button></div></div>
      <div class="card toolbar"><input id="o-q" class="input" placeholder="Search customer, id or product" aria-label="Search orders">
        <div class="seg" id="o-f">${['ALL'].concat(RR.PRIORITIES).map(p => `<button class="${p === 'ALL' ? 'on' : ''}" data-v="${p}">${p}</button>`).join('')}</div></div>
      <div id="o-list" class="olist"></div>`;
    function list() {
      const st = S();
      const rows = st.orders.filter(o => (filter === 'ALL' || o.priority === filter) &&
        (!q || (o.id + ' ' + o.customer + ' ' + o.lines.map(l => l.product).join(' ')).toLowerCase().includes(q)));
      $('#o-list', view).innerHTML = rows.map((o, i) => `
        <div class="card order-row" style="animation-delay:${Math.min(i, 12) * 40}ms">
          <div class="o-id p-${o.priority}">${esc(o.id)}</div>
          <div class="o-main"><b>${esc(o.customer)}</b>
            <span class="muted">to ${esc(nodeName(o.dest))} &middot; arrives t=${o.arrival} &middot; ${o.deadline < 0 ? 'no deadline' : 'deadline t=' + o.deadline}</span>
            <div class="items">${o.lines.map(l => `<span class="tag">${esc(l.product)} x${l.qty}</span>`).join('')}</div></div>
          <div>${chip(o.priority)}</div>
          <button class="icon-btn danger" data-del="${esc(o.id)}" aria-label="Remove order ${esc(o.id)}" title="Remove">${RR.icon('trash', 16)}</button>
        </div>`).join('') ||
        `<div class="card empty"><div class="empty-ico">${RR.icon('box', 36)}</div><h3>${st.orders.length ? 'No orders match' : 'No orders yet'}</h3><p class="muted">${st.orders.length ? 'Try another filter.' : 'Place one to see it move through the warehouse.'}</p></div>`;
    }
    $('#o-q', view).oninput = e => { q = e.target.value.trim().toLowerCase(); list(); };
    $('#o-f', view).onclick = e => { const b = e.target.closest('button'); if (!b) return; $$('#o-f button', view).forEach(x => x.classList.remove('on')); b.classList.add('on'); filter = b.dataset.v; list(); };
    $('#o-new', view).onclick = () => RR.openOrderModal();
    $('#o-reset', view).onclick = async () => {
      if (!(await RR.confirm('Reset all data?', 'Orders and stock go back to what was loaded from the CSV files. Anything you added is removed.', 'Reset'))) return;
      try { await RR.api('/api/reset', 'POST'); RR.store.run = null; RR.store.compare = null; await RR.loadState(); RR.toast('Data reset', 'ok'); RR.log('Data reset to the CSV files', 'info'); } catch (e) { RR.toast(e.message, 'err'); }
    };
    $('#o-list', view).onclick = async e => {
      const b = e.target.closest('[data-del]'); if (!b) return;
      const row = b.closest('.order-row'); row.classList.add('leaving');
      try { await RR.api('/api/order/delete', 'POST', { id: b.dataset.del }); RR.log(`Order ${b.dataset.del} removed`, 'info'); setTimeout(() => RR.loadState(), 220); }
      catch (err) { row.classList.remove('leaving'); RR.toast(err.message, 'err'); }
    };
    const off = RR.bus.on('state', list);
    list();
    return () => off();
  };

  // ================================================================== INVENTORY
  RR.pages.inventory = function (view) {
    view.innerHTML = `
      <div class="page-head"><div><h1>Inventory</h1><p class="muted">Hash-map backed stock. The marker shows how many units the current orders want.</p></div>
        <div class="row"><button class="btn primary" id="i-new">${RR.icon('plus', 16)} New product</button></div></div>
      <div class="grid products" id="i-grid"></div>`;
    function grid() {
      const st = S(), demand = RR.demand(st), maxQ = Math.max(1, ...st.products.map(p => Math.max(p.qty, demand[p.id] || 0)));
      $('#i-grid', view).innerHTML = st.products.map((p, i) => {
        const d = demand[p.id] || 0, short = d > p.qty;
        return `<div class="card prod tilt" style="animation-delay:${Math.min(i, 12) * 45}ms">
          <div class="prod-top"><span class="tag">${esc(p.id)}</span><span class="muted">${esc(nodeName(p.node))}</span></div>
          <h3>${esc(p.name)}</h3>
          <div class="prod-qty"><b>${p.qty}</b><span class="muted">in stock &middot; ${p.weight} kg each</span></div>
          <div class="meter tall"><i class="${short ? 'low' : (p.qty / maxQ < 0.25 ? 'warn' : '')}" style="width:${(p.qty / maxQ * 100).toFixed(1)}%"></i><u style="left:${(d / maxQ * 100).toFixed(1)}%" title="requested by orders"></u></div>
          <div class="${short ? 'bad' : 'muted'} small">${d} requested${short ? ', short by ' + (d - p.qty) : ''}</div>
          <div class="row prod-actions"><button class="btn sm" data-r="10" data-id="${esc(p.id)}">+10</button><button class="btn sm" data-r="50" data-id="${esc(p.id)}">+50</button><button class="btn sm ghost" data-rc="${esc(p.id)}">Custom</button></div>
        </div>`;
      }).join('') || '<div class="card empty"><h3>No products</h3></div>';
    }
    async function restock(id, n) {
      try { await RR.api('/api/restock', 'POST', { id, qty: n }); RR.toast(`${id} +${n}`, 'ok'); RR.log(`Restocked ${id} by ${n}`, 'ok'); await RR.loadState(); }
      catch (e) { RR.toast(e.message, 'err'); }
    }
    $('#i-grid', view).onclick = e => {
      const b = e.target.closest('button'); if (!b) return;
      if (b.dataset.r) restock(b.dataset.id, b.dataset.r);
      else if (b.dataset.rc) {
        const id = b.dataset.rc;
        RR.modal({ title: 'Restock ' + id, body: `<form class="form" id="rf"><label class="field">Units to add<input class="input" id="rf-q" type="number" min="1" value="25" required></label><div class="form-error" id="rf-e"></div><div class="row end"><button class="btn primary" type="submit">Add stock</button></div></form>`,
          onMount(el, api) { $('#rf', el).onsubmit = async ev => { ev.preventDefault(); try { await RR.api('/api/restock', 'POST', { id, qty: $('#rf-q', el).value }); RR.log(`Restocked ${id} by ${$('#rf-q', el).value}`, 'ok'); await RR.loadState(); api.close(); } catch (er) { $('#rf-e', el).textContent = er.message; } }; } });
      }
    };
    $('#i-new', view).onclick = () => {
      const st = S(), nodes = st.nodes.filter(n => n.id !== st.depot && n.id !== st.packing);
      RR.modal({
        title: 'New product',
        body: `<form class="form" id="pf" autocomplete="off">
          <div class="row"><label class="field">ID<input class="input" id="pf-id" maxlength="12" placeholder="P007" required></label><label class="field grow">Name<input class="input" id="pf-name" maxlength="30" placeholder="Headphones" required></label></div>
          <div class="row"><label class="field">Quantity<input class="input" id="pf-q" type="number" min="0" value="20"></label><label class="field">kg per unit<input class="input" id="pf-w" type="number" min="0.01" step="0.01" value="1"></label>
          <label class="field grow">Shelf<select class="input" id="pf-n">${nodes.map(n => `<option value="${n.id}">${esc(n.name)}</option>`).join('')}</select></label></div>
          <div class="form-error" id="pf-e" role="alert"></div><div class="row end"><button class="btn primary" type="submit">Add product</button></div></form>`,
        onMount(el, api) {
          $('#pf', el).onsubmit = async ev => {
            ev.preventDefault();
            try {
              await RR.api('/api/product', 'POST', { id: $('#pf-id', el).value, name: $('#pf-name', el).value, qty: $('#pf-q', el).value, node: $('#pf-n', el).value, weight: $('#pf-w', el).value });
              RR.toast('Product added', 'ok'); RR.log(`Product ${$('#pf-id', el).value} added`, 'ok'); await RR.loadState(); api.close();
            } catch (e) { $('#pf-e', el).textContent = e.message; }
          };
        }
      });
    };
    const off = RR.bus.on('state', grid);
    grid();
    return () => off();
  };

  // ================================================================== SIMULATION LAB
  function ganttSvg(run) {
    const rows = RR.ganttRows(run), maxT = Math.max(10, RR.makespanOf(run)), left = 54, W = 940, rowH = 24, top = 22, H = top + rows.length * rowH + 22;
    const x = t => left + (t / maxT) * (W - left - 12);
    let s = `<svg class="gantt" viewBox="0 0 ${W} ${H}" preserveAspectRatio="xMidYMin meet" role="img" aria-label="Timeline of every order">`;
    const step = maxT > 400 ? 100 : (maxT > 150 ? 50 : 20);
    for (let t = 0; t <= maxT; t += step) s += `<line x1="${x(t)}" y1="${top - 6}" x2="${x(t)}" y2="${H - 18}" class="g-grid"/><text x="${x(t)}" y="${top - 9}" class="g-axis" text-anchor="middle">${t}</text>`;
    rows.forEach((r, i) => {
      const y = top + i * rowH, col = RR.PRIO_COLOR[r.priority];
      s += `<text x="${left - 8}" y="${y + 15}" class="g-lab" text-anchor="end">${esc(r.id)}</text>`;
      s += `<line x1="${x(r.arrival)}" y1="${y + 11}" x2="${x(r.start)}" y2="${y + 11}" class="g-wait"/>`;
      if (r.status === 'DELIVERED') {
        s += `<rect x="${x(r.start)}" y="${y + 4}" width="${Math.max(2, x(r.pickEnd) - x(r.start))}" height="14" rx="3" fill="${col}" opacity=".95"><title>${esc(r.id)} picking</title></rect>`;
        s += `<rect x="${x(r.dispatch)}" y="${y + 6}" width="${Math.max(2, x(r.done) - x(r.dispatch))}" height="10" rx="3" fill="${col}" opacity=".5" stroke="${r.late ? '#d4483b' : 'none'}" stroke-width="1.5"><title>${esc(r.id)} on the road (${esc(r.vehicles)})</title></rect>`;
        s += `<rect x="${x(r.done)}" y="${y + 8}" width="${Math.max(2, x(r.back) - x(r.done))}" height="6" rx="3" fill="${col}" opacity=".18"/>`;
        if (r.deadline >= 0) s += `<path d="M${x(r.deadline)} ${y + 2} l4 4 l-4 4 l-4 -4 z" class="g-dead"><title>deadline ${r.deadline}</title></path>`;
      } else s += `<text x="${x(r.start) + 4}" y="${y + 16}" class="g-rej">rejected</text>`;
    });
    s += `<line id="g-play" x1="${x(0)}" y1="${top - 6}" x2="${x(0)}" y2="${H - 18}" class="g-play"/></svg>`;
    return { svg: s, x, maxT };
  }

  RR.pages.simulate = function (view) {
    let run = null, t = 0, speed = 1, playing = false, raf = 0, last = 0, events = [], gx = null, shownEvents = -1;
    const cfg = { fifo: '0', pick: '2opt' };
    view.innerHTML = `
      <div class="page-head"><div><h1>Simulation Lab</h1><p class="muted">Pick a policy, run it, and watch pickers and vehicles follow the routes the backend computed.</p></div></div>
      <div class="card controls">
        <div class="field">Queue policy<div class="seg" id="s-q"><button class="on" data-v="0">Priority + aging</button><button data-v="1">FIFO (baseline)</button></div></div>
        <div class="field">Picking route<div class="seg" id="s-p"><button data-v="naive">Naive</button><button data-v="nn">Nearest neighbour</button><button class="on" data-v="2opt">NN + 2-opt</button></div></div>
        <button class="btn primary big" id="s-run">${RR.icon('play', 18)} Run simulation</button>
      </div>
      <div class="card scene-card"><div class="scene-box tall" id="s-scene"></div>
        <div class="hud"><div class="hud-tick">TICK <b id="h-t">0</b> <span class="muted">/ <span id="h-m">0</span></span></div><div class="hud-now" id="h-now">Press Run to start.</div></div>
        <div class="legend"><span><i style="background:#8fae6b"></i>Bike</span><span><i style="background:#e8b04a"></i>Van</span><span><i style="background:#e0793f"></i>Truck</span><span><i style="background:#fff"></i>Picker</span></div></div>
      <div class="card playbar" id="s-bar" hidden>
        <button class="btn" id="s-pp" aria-label="Play or pause">${RR.icon('pause', 16)}</button><button class="btn" id="s-rs" aria-label="Restart">${RR.icon('restart', 16)}</button>
        <div class="seg sm" id="s-sp"><button data-v="0.5">0.5x</button><button class="on" data-v="1">1x</button><button data-v="2">2x</button><button data-v="4">4x</button></div>
        <input type="range" id="s-scrub" min="0" max="100" step="0.1" value="0" aria-label="Timeline position"></div>
      <div class="grid cols-2" id="s-lower" hidden>
        <div class="card"><h2>Timeline</h2><div class="gantt-wrap" id="s-gantt"></div>
          <div class="legend"><span><i class="g-sw wait"></i>queue wait</span><span><i class="g-sw pick"></i>picking</span><span><i class="g-sw road"></i>on the road</span><span><i class="g-sw back"></i>return</span><span><i class="g-sw dl"></i>deadline</span></div></div>
        <div class="card"><h2>Live events</h2><ul class="feed ticker" id="s-ticker"></ul></div>
      </div>
      <div id="s-results"></div>`;
    const scene = mountScene($('#s-scene', view), { autoRotate: true, labels: true, pulses: false });

    function segBind(id, key) {
      $('#' + id, view).onclick = e => { const b = e.target.closest('button'); if (!b) return; $$('#' + id + ' button', view).forEach(x => x.classList.remove('on')); b.classList.add('on'); cfg[key] = b.dataset.v; };
    }
    segBind('s-q', 'fifo'); segBind('s-p', 'pick');

    function hudNow(actors) {
      const el = $('#h-now', view);
      if (!run) return;
      if (t >= RR.makespanOf(run)) { el.textContent = 'Run complete.'; return; }
      const ps = actors.filter(a => a.kind === 'picker').map(a => 'Picking ' + a.orderId);
      const vs = actors.filter(a => a.kind === 'vehicle' && a.dir === 'out').map(a => a.vehicle + ' ' + a.vehicleId + ' delivering ' + a.orderId);
      el.textContent = ps.concat(vs).slice(0, 3).join('  |  ') || 'Waiting for the next order...';
    }
    function ticker() {
      const done = events.filter(e => e.t <= t);
      if (done.length === shownEvents) return;
      shownEvents = done.length;
      $('#s-ticker', view).innerHTML = done.slice(-9).reverse().map(e => `<li class="${e.kind}"><time>t=${e.t}</time><span>${esc(e.text)}</span></li>`).join('') || '<li class="muted">Waiting...</li>';
    }
    function frame(now) {
      raf = requestAnimationFrame(frame);
      if (!view.isConnected) { cancelAnimationFrame(raf); return; }
      const dt = Math.min(0.1, (now - last) / 1000); last = now;
      if (run && playing) {
        t += dt * speed * TICKS_PER_SECOND;
        const end = RR.makespanOf(run);
        if (t >= end) { t = end; playing = false; $('#s-pp', view).innerHTML = RR.icon('play', 16); RR.toast('Run complete', 'ok'); }
      }
      if (run) {
        const actors = RR.simActors(run, t);
        if (scene) scene.updateActors(actors);
        $('#h-t', view).textContent = Math.round(t);
        const end = RR.makespanOf(run);
        $('#s-scrub', view).value = end ? (t / end) * 100 : 0;
        if (gx) { const px = gx.x(t); const pl = $('#g-play', view); if (pl) { pl.setAttribute('x1', px); pl.setAttribute('x2', px); } }
        hudNow(actors); ticker();
      }
    }
    function results() {
      const m = run.metrics, recs = run.records;
      $('#s-results', view).innerHTML = `
        <div class="grid kpis">
          <div class="card kpi" style="--accent:var(--green)"><div class="kpi-label">Delivered</div><div class="kpi-val">${m.delivered}</div><div class="kpi-sub">${m.rejected} rejected</div></div>
          <div class="card kpi" style="--accent:${m.late ? 'var(--pink)' : 'var(--green)'}"><div class="kpi-label">Late</div><div class="kpi-val">${m.late}</div><div class="kpi-sub">past their deadline</div></div>
          <div class="card kpi" style="--accent:var(--cyan)"><div class="kpi-label">Makespan</div><div class="kpi-val">${m.makespan}</div><div class="kpi-sub">ticks to finish all</div></div>
          <div class="card kpi" style="--accent:var(--yellow)"><div class="kpi-label">Total cost</div><div class="kpi-val">${RR.fmt1(m.totalCost)}</div><div class="kpi-sub">avg pick ${RR.fmt1(m.avgPick)}</div></div>
        </div>
        <div class="card"><h2>Delivery log</h2><div class="scroll"><table class="tbl"><thead><tr><th>ID</th><th>Customer</th><th>Priority</th><th>Status</th><th class="num">Pick</th><th class="num">Deliv</th><th>Vehicles</th><th class="num">Cost</th><th class="num">Start</th><th class="num">Done</th><th>Note</th></tr></thead><tbody>
        ${recs.map(r => `<tr><td>${esc(r.id)}</td><td>${esc(r.customer)}</td><td>${chip(r.priority)}</td><td class="st-${r.status}">${r.status}</td><td class="num">${r.pick}</td><td class="num">${r.deliv}</td><td>${esc(r.vehicles)}</td><td class="num">${r.cost}</td><td class="num">${r.start}</td><td class="num">${r.done}</td><td>${r.late ? '<span class="bad">LATE </span>' : ''}${esc(r.note)}</td></tr>`).join('')}
        </tbody></table></div><p class="hint">Vehicles drawn in the 3D view leave when picking is done (or when a vehicle frees up) and return after delivery. Split loads are shown leaving together.</p></div>`;
    }
    async function doRun() {
      const st = S();
      if (!st.orders.length) { RR.toast('There are no orders to run. Add one first.', 'err'); return; }
      const btn = $('#s-run', view); btn.disabled = true; btn.classList.add('busy');
      try {
        run = await RR.api('/api/run', 'POST', { fifo: cfg.fifo, pick: cfg.pick });
        RR.store.run = run; t = 0; playing = true; shownEvents = -1; events = RR.runEvents(run);
        if (scene) scene.clearActors();
        const g = ganttSvg(run); gx = g; $('#s-gantt', view).innerHTML = g.svg;
        $('#h-m', view).textContent = RR.makespanOf(run);
        $('#s-bar', view).hidden = false; $('#s-lower', view).hidden = false;
        $('#s-pp', view).innerHTML = RR.icon('pause', 16);
        results();
        RR.log(`Ran ${run.policy.fifo ? 'FIFO' : 'priority + aging'} / ${run.policy.pick}: ${run.metrics.delivered} delivered, ${run.metrics.rejected} rejected, ${run.metrics.late} late`, 'ok');
        RR.bus.emit('feed');
      } catch (e) { RR.toast(e.message, 'err'); } finally { btn.disabled = false; btn.classList.remove('busy'); }
    }
    $('#s-run', view).onclick = doRun;
    $('#s-pp', view).onclick = () => {
      if (!run) return;
      if (t >= RR.makespanOf(run)) t = 0;
      playing = !playing; $('#s-pp', view).innerHTML = RR.icon(playing ? 'pause' : 'play', 16);
    };
    $('#s-rs', view).onclick = () => { if (!run) return; t = 0; playing = true; shownEvents = -1; $('#s-pp', view).innerHTML = RR.icon('pause', 16); };
    $('#s-sp', view).onclick = e => { const b = e.target.closest('button'); if (!b) return; $$('#s-sp button', view).forEach(x => x.classList.remove('on')); b.classList.add('on'); speed = parseFloat(b.dataset.v); };
    $('#s-scrub', view).oninput = e => { if (!run) return; t = (parseFloat(e.target.value) / 100) * RR.makespanOf(run); playing = false; $('#s-pp', view).innerHTML = RR.icon('play', 16); shownEvents = -1; };

    last = performance.now(); raf = requestAnimationFrame(frame);
    const offState = RR.bus.on('state', () => { if (scene) scene.setGraph(RR.sceneGraph(S())); });
    if (RR.pending.run) { RR.pending.run = false; setTimeout(doRun, 350); }
    else if (RR.store.run) { /* keep results out of sight until the user presses Run: they may be stale */ }
    return () => { cancelAnimationFrame(raf); offState(); if (scene) scene.destroy(); };
  };

  // ================================================================== COMPARE
  RR.pages.compare = function (view) {
    view.innerHTML = `
      <div class="page-head"><div><h1>Compare policies</h1><p class="muted">The same orders, run twice: our design versus a plain FIFO queue with typed-order picking.</p></div>
        <div class="row"><button class="btn primary" id="c-run">${RR.icon('compare', 16)} Re-run comparison</button></div></div>
      <div id="c-body"><div class="card empty"><h3>Running both policies...</h3></div></div>`;
    let dead = false;
    function barChart(a, b) {
      const W = 560, H = 270, base = 220, top = 28, gw = 150, mx = Math.max(1, ...RR.PRIORITIES.map(p => Math.max(a.metrics.wait[p].avg, b.metrics.wait[p].avg)));
      let s = `<svg class="chart" viewBox="0 0 ${W} ${H}" role="img" aria-label="Average wait by priority, ours versus baseline">`;
      for (let i = 0; i <= 4; i++) { const y = base - (i / 4) * (base - top); s += `<line x1="40" x2="${W - 10}" y1="${y}" y2="${y}" class="g-grid"/><text x="34" y="${y + 4}" class="g-axis" text-anchor="end">${Math.round(mx * i / 4)}</text>`; }
      RR.PRIORITIES.forEach((p, gi) => {
        const x0 = 60 + gi * gw, va = a.metrics.wait[p].avg, vb = b.metrics.wait[p].avg;
        const ha = (va / mx) * (base - top), hb = (vb / mx) * (base - top);
        s += `<rect class="cbar" x="${x0}" y="${base - ha}" width="44" height="${ha}" rx="4" fill="url(#gA)" style="animation-delay:${gi * 120}ms"/><text x="${x0 + 22}" y="${base - ha - 6}" class="c-val" text-anchor="middle">${RR.fmt1(va)}</text>`;
        s += `<rect class="cbar" x="${x0 + 52}" y="${base - hb}" width="44" height="${hb}" rx="4" fill="url(#gB)" style="animation-delay:${gi * 120 + 60}ms"/><text x="${x0 + 74}" y="${base - hb - 6}" class="c-val" text-anchor="middle">${RR.fmt1(vb)}</text>`;
        s += `<text x="${x0 + 48}" y="${base + 20}" class="c-cat" text-anchor="middle" fill="${RR.PRIO_COLOR[p]}">${p}</text>`;
      });
      s += `<defs><linearGradient id="gA" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#e0793f"/><stop offset="1" stop-color="#b4501f"/></linearGradient><linearGradient id="gB" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#a8917a"/><stop offset="1" stop-color="#8c7860"/></linearGradient></defs></svg>`;
      return s;
    }
    function metricBars(a, b) {
      const items = [['Late deliveries', a.metrics.late, b.metrics.late, 0], ['Makespan (ticks)', a.metrics.makespan, b.metrics.makespan, 0], ['Avg turnaround', a.metrics.avgTurnaround, b.metrics.avgTurnaround, 1], ['Avg pick distance', a.metrics.avgPick, b.metrics.avgPick, 1], ['Total cost', a.metrics.totalCost, b.metrics.totalCost, 0]];
      return items.map(([name, x, y, dec]) => {
        const mx = Math.max(x, y, 1e-9), better = x < y ? 'ours' : (y < x ? 'base' : 'same');
        return `<div class="mrow"><div class="mrow-h"><b>${name}</b><span class="muted">${better === 'ours' ? 'ours is lower' : better === 'base' ? 'baseline is lower' : 'equal'}</span></div>
          <div class="hbar"><span class="lab a">ours</span><div class="meter"><i class="a" style="width:${(x / mx * 100).toFixed(1)}%"></i></div><b>${dec ? RR.fmt1(x) : x}</b></div>
          <div class="hbar"><span class="lab b">base</span><div class="meter"><i class="b" style="width:${(y / mx * 100).toFixed(1)}%"></i></div><b>${dec ? RR.fmt1(y) : y}</b></div></div>`;
      }).join('');
    }
    async function load() {
      const body = $('#c-body', view);
      if (!S().orders.length) { body.innerHTML = '<div class="card empty"><h3>No orders to compare</h3><p class="muted">Add some orders first.</p><a class="btn primary" href="#/orders">Go to orders</a></div>'; return; }
      body.innerHTML = '<div class="card empty"><div class="spinner"></div><h3>Running both policies...</h3></div>';
      let c;
      try { c = await RR.api('/api/compare'); } catch (e) { if (!dead) body.innerHTML = `<div class="card empty"><h3>Could not compare</h3><p class="bad">${esc(e.message)}</p></div>`; return; }
      if (dead) return;
      RR.store.compare = c;
      const [a, b] = c.runs, v = RR.compareVerdict(a, b);
      const good = v.filter(x => x.tone === 'good'), bad = v.filter(x => x.tone === 'bad'), info = v.filter(x => x.tone === 'info');
      body.innerHTML = `
        <div class="grid cols-2">
          <div class="card verdict good"><h2>What our design wins</h2><ul>${good.map(x => `<li>${esc(x.text)}</li>`).join('') || '<li class="muted">Nothing on this data.</li>'}</ul></div>
          <div class="card verdict bad"><h2>What it costs</h2><ul>${bad.map(x => `<li>${esc(x.text)}</li>`).join('') || '<li class="muted">No metric got worse.</li>'}</ul>${info.length ? `<p class="muted small">${info.map(x => esc(x.text)).join(' ')}</p>` : ''}</div>
        </div>
        <div class="grid cols-2">
          <div class="card"><h2>Average wait by priority (ticks)</h2><div class="legend"><span><i style="background:#e0793f"></i>${esc(a.name)}</span><span><i style="background:#a8917a"></i>${esc(b.name)}</span></div>${barChart(a, b)}</div>
          <div class="card"><h2>Other metrics</h2>${metricBars(a, b)}</div>
        </div>
        <div class="card"><h2>Reading these results honestly</h2>
          <p class="muted">Priority with aging serves urgent orders first, so they wait far less, but orders behind them wait longer: the total waiting work is shifted, not removed. Aging stops the slowest orders from waiting forever, yet on a small overloaded set like this one, less important orders can still wait more than under FIFO. The orders here are synthetic and few, so treat the direction as the finding, not the exact percentages.</p></div>`;
    }
    $('#c-run', view).onclick = load;
    load();
    return () => { dead = true; };
  };

  // ================================================================== MAP
  RR.pages.map = function (view, params) {
    view.innerHTML = `
      <div class="page-head"><div><h1>Warehouse map</h1><p class="muted">The graph the backend runs Dijkstra on. Distances in the layout file become edge lengths here.</p></div>
        <div class="row"><button class="btn ghost" id="m-reset">Reset view</button></div></div>
      <div class="grid map-grid">
        <div class="card scene-card"><div class="scene-box xl" id="m-scene"></div>
          <div class="chips" id="m-toggles"><label class="toggle"><input type="checkbox" data-o="labels" checked><span>Labels</span></label><label class="toggle"><input type="checkbox" data-o="weights"><span>Edge weights</span></label><label class="toggle"><input type="checkbox" data-o="pulses" checked><span>Energy pulses</span></label><label class="toggle"><input type="checkbox" data-o="autoRotate" checked><span>Auto-rotate</span></label></div></div>
        <div class="col">
          <div class="card" id="m-detail"></div>
          <div class="card"><h2>Route finder</h2><p class="muted small">Shortest path between two nodes (Dijkstra).</p>
            <div class="row"><label class="field grow">From<select class="input" id="m-from"></select></label><label class="field grow">To<select class="input" id="m-to"></select></label></div>
            <div class="row"><button class="btn primary" id="m-go">Show route</button><button class="btn ghost" id="m-clear">Clear</button></div><div id="m-route" class="route-out"></div></div>
        </div>
      </div>`;
    const st0 = S();
    const opts = st0.nodes.map(n => `<option value="${n.id}">${esc(n.name)}</option>`).join('');
    $('#m-from', view).innerHTML = opts; $('#m-to', view).innerHTML = opts;
    $('#m-from', view).value = String(st0.packing); $('#m-to', view).value = String(st0.nodes.length ? st0.nodes[st0.nodes.length - 1].id : 0);
    let selected = null;
    const scene = mountScene($('#m-scene', view), { autoRotate: true, labels: true, weights: false, pulses: true, onSelect: id => select(id) });

    function select(id) {
      selected = id;
      if (scene) scene.setSelected(id);
      const st = S(), box = $('#m-detail', view);
      if (id === null || !st.nodes[id]) { box.innerHTML = '<h2>Node details</h2><p class="muted">Click a node in the map to see what it stores, who it serves, and what it connects to.</p>'; return; }
      const n = st.nodes[id], type = RR.nodeType(st, id), prods = st.products.filter(p => p.node === id), ords = st.orders.filter(o => o.dest === id), nb = RR.neighbours(st, id);
      box.innerHTML = `<div class="card-head"><h2>${esc(n.name)}</h2><span class="tag t-${type}">${type === 'city' ? 'customer area' : type}</span></div>
        <p class="muted">${n.dist === null ? 'Unreachable from Packing.' : 'Distance from Packing: <b>' + RR.fmt1(n.dist) + '</b>'}</p>
        ${prods.length ? `<h4>Stores</h4><ul class="mini">${prods.map(p => `<li>${esc(p.name)} <b>${p.qty}</b></li>`).join('')}</ul>` : ''}
        ${ords.length ? `<h4>Orders delivered here</h4><ul class="mini">${ords.map(o => `<li>${esc(o.id)} ${esc(o.customer)} ${chip(o.priority)}</li>`).join('')}</ul>` : ''}
        <h4>Connected to</h4><ul class="mini links">${nb.map(x => `<li><a href="#" data-n="${x.id}">${esc(st.nodes[x.id].name)}</a> <b>${RR.fmt1(x.w)}</b></li>`).join('') || '<li class="muted">nothing</li>'}</ul>`;
    }
    $('#m-detail', view).onclick = e => { const a = e.target.closest('[data-n]'); if (!a) return; e.preventDefault(); const id = parseInt(a.dataset.n, 10); select(id); if (scene) scene.focusNode(id); };
    $('#m-reset', view).onclick = () => { if (scene) scene.resetView(); };
    $('#m-toggles', view).onchange = e => {
      const o = e.target.dataset.o; if (!o || !scene) return;
      if (o === 'autoRotate') scene.opts.autoRotate = e.target.checked; else scene.setOption(o, e.target.checked);
    };
    $('#m-go', view).onclick = () => {
      const a = parseInt($('#m-from', view).value, 10), b = parseInt($('#m-to', view).value, 10), r = RR.shortestPath(S(), a, b), out = $('#m-route', view);
      if (!r) { out.innerHTML = '<p class="bad">No route between those nodes.</p>'; if (scene) scene.highlightPath(null); return; }
      if (scene) scene.highlightPath(r.path);
      out.innerHTML = `<p><b>${RR.fmt1(r.dist)}</b> units</p><div class="path">${r.path.map(i => `<span>${esc(nodeName(i))}</span>`).join('<em>&rarr;</em>')}</div>`;
    };
    $('#m-clear', view).onclick = () => { if (scene) scene.highlightPath(null); $('#m-route', view).innerHTML = ''; };
    const off = RR.bus.on('state', () => { if (scene) scene.setGraph(RR.sceneGraph(S())); select(selected); });
    const want = params && params.node !== undefined ? parseInt(params.node, 10) : null;
    select(want !== null && !Number.isNaN(want) ? want : null);
    if (want !== null && scene) setTimeout(() => scene.focusNode(want), 300);
    return () => { off(); if (scene) scene.destroy(); };
  };

  // ================================================================== ABOUT
  RR.pages.about = function (view) {
    const steps = [
      ['Order arrives', 'Validated and queued', 'Types, Session', 'var(--cyan)'],
      ['Priority queue', 'Binary heap with aging', 'OrderQueue.h', 'var(--pink)'],
      ['Stock check', 'Chained hash map, all-or-nothing reserve', 'HashMap.h, Inventory.h', 'var(--yellow)'],
      ['Picking route', 'Dijkstra, nearest neighbour, 2-opt', 'Graph.h, RoutePlanner.h', 'var(--green)'],
      ['Vehicle', 'Cheapest fit, split loads', 'Fleet.h, Vehicle.h', 'var(--cyan)'],
      ['Delivery log', 'CSV records and metrics', 'WarehouseManager.h, FileStore.h', 'var(--pink)']
    ];
    const team = [
      ['Sundram Sharma', 'Team lead', 'Pipeline, fleet, state layer, API, server, menu'],
      ['Samriddhi Agarwal', 'Data structures', 'Hash map, inventory, aging queue, HTTP parsing'],
      ['Arya Kumari', 'Graphs and routes', 'Graph, Dijkstra, route planner, web pages'],
      ['Ishita Sirohi', 'Data and testing', 'CSV stores, benchmarks, test framework, API tests']
    ];
    view.innerHTML = `
      <div class="page-head"><div><h1>How RouteRush works</h1><p class="muted">Team 404 CREW &middot; DSCPP-III-2026-T090 &middot; C++17, no external libraries in the backend.</p></div></div>
      <div class="card"><h2>Order to delivery</h2><div class="flow">${steps.map((s, i) => `<div class="step tilt" style="--accent:${s[3]};animation-delay:${i * 90}ms"><span class="n">${i + 1}</span><b>${s[0]}</b><p>${s[1]}</p><code>${s[2]}</code></div>${i < steps.length - 1 ? '<div class="arrow"><i></i></div>' : ''}`).join('')}</div></div>
      <div class="grid cols-2">
        <div class="card"><h2>Team</h2><div class="team">${team.map(m => `<div class="member tilt"><div class="avatar">${esc(m[0].split(' ').map(w => w[0]).join(''))}</div><div><b>${esc(m[0])}</b><span class="muted">${esc(m[1])}</span><p>${esc(m[2])}</p></div></div>`).join('')}</div></div>
        <div class="card"><h2>Honest limits</h2><ul class="limits">
          <li>All data is synthetic and small, so results show direction, not real-world rates.</li>
          <li>Picking order uses heuristics (nearest neighbour, 2-opt). They are never proven optimal.</li>
          <li>Each simulation replays the whole order list; it is not a live system.</li>
          <li>The server has no login and listens on this computer only. Do not expose it to a network.</li>
          <li>Vehicles are drawn leaving when the order is delivered minus travel time. Split loads are shown departing together.</li></ul></div>
      </div>`;
    return () => {};
  };
})(window);
