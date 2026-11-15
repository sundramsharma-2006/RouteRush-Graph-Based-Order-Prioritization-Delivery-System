/* RouteRush web - core: API client, shared state, and the pure maths used by the 3D scene and playback.
   Everything that is not DOM/WebGL lives here so it can be unit-tested in Node (see tests in the repo notes). */
(function (root) {
  'use strict';
  const RR = root.RR = root.RR || {};

  // ------------------------------------------------------------------ small utilities
  RR.esc = s => String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  RR.clamp = (v, a, b) => Math.min(b, Math.max(a, v));
  RR.lerp = (a, b, t) => a + (b - a) * t;
  RR.ease = t => (t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2);
  RR.PRIORITIES = ['URGENT', 'EXPRESS', 'NORMAL'];
  RR.PRIO_COLOR = { URGENT: '#d4483b', EXPRESS: '#e8b04a', NORMAL: '#8fae8b' };
  RR.fmt1 = v => (v === null || v === undefined || Number.isNaN(v) ? '-' : (Math.round(v * 10) / 10).toFixed(1));

  function mulberry32(seed) {                       // tiny deterministic random generator
    let a = seed >>> 0;
    return function () {
      a = (a + 0x6D2B79F5) >>> 0;
      let t = a;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }
  RR.mulberry32 = mulberry32;

  // ------------------------------------------------------------------ 3D layout of the warehouse graph
  // Force-directed ("spring") layout: connected nodes pull towards a rest length proportional to the edge
  // weight, every pair of nodes repels. Deterministic for a given seed, so the map looks the same every visit.
  RR.springLayout = function (nodes, edges, opts) {
    opts = opts || {};
    const n = nodes.length;
    if (n === 0) return [];
    const scale = opts.scale || 0.45, iters = opts.iters || 700, radius = opts.radius || 13, flat = opts.flat === undefined ? 0.5 : opts.flat;
    const rnd = mulberry32(opts.seed || 7);
    const pos = nodes.map(() => {
      const a = rnd() * Math.PI * 2, b = Math.acos(2 * rnd() - 1), r = 5 + rnd() * 3;
      return [r * Math.sin(b) * Math.cos(a), r * Math.cos(b), r * Math.sin(b) * Math.sin(a)];
    });
    if (n === 1) return [[0, 0, 0]];
    const rest = edges.map(e => Math.max(1.5, e.w * scale));
    let temp = 1;
    for (let it = 0; it < iters; it++) {
      const f = pos.map(() => [0, 0, 0]);
      for (let i = 0; i < n; i++) {
        for (let j = i + 1; j < n; j++) {
          const dx = pos[i][0] - pos[j][0], dy = pos[i][1] - pos[j][1], dz = pos[i][2] - pos[j][2];
          const d = Math.max(0.2, Math.sqrt(dx * dx + dy * dy + dz * dz));
          const k = 30 / (d * d * d);                     // repulsion / d, times the vector -> 1/d^2 force
          f[i][0] += dx * k; f[i][1] += dy * k; f[i][2] += dz * k;
          f[j][0] -= dx * k; f[j][1] -= dy * k; f[j][2] -= dz * k;
        }
      }
      edges.forEach((e, idx) => {
        const dx = pos[e.b][0] - pos[e.a][0], dy = pos[e.b][1] - pos[e.a][1], dz = pos[e.b][2] - pos[e.a][2];
        const d = Math.max(0.01, Math.sqrt(dx * dx + dy * dy + dz * dz));
        const s = (d - rest[idx]) * 0.15 / d;
        f[e.a][0] += dx * s; f[e.a][1] += dy * s; f[e.a][2] += dz * s;
        f[e.b][0] -= dx * s; f[e.b][1] -= dy * s; f[e.b][2] -= dz * s;
      });
      for (let i = 0; i < n; i++) {                       // weak pull to the centre, then move (capped step)
        for (let c = 0; c < 3; c++) f[i][c] -= pos[i][c] * 0.01;
        const mag = Math.sqrt(f[i][0] * f[i][0] + f[i][1] * f[i][1] + f[i][2] * f[i][2]);
        const cap = Math.min(1, 0.6 / Math.max(mag, 1e-9)) * temp;
        for (let c = 0; c < 3; c++) pos[i][c] += f[i][c] * cap;
      }
      temp *= 0.994;
    }
    // centre, flatten (a warehouse is mostly a floor plan), and fit into the requested radius
    const mean = [0, 0, 0];
    pos.forEach(p => { for (let c = 0; c < 3; c++) mean[c] += p[c] / n; });
    pos.forEach(p => { for (let c = 0; c < 3; c++) p[c] -= mean[c]; p[1] *= flat; });
    let maxR = 0;
    pos.forEach(p => { maxR = Math.max(maxR, Math.sqrt(p[0] * p[0] + p[2] * p[2])); });
    const k = maxR > 1e-6 ? radius / maxR : 1;
    return pos.map(p => [p[0] * k, p[1] * k, p[2] * k]);
  };

  // ------------------------------------------------------------------ walking along a poly-line of 3D points
  RR.polyLen = function (pts) {
    let L = 0;
    for (let i = 0; i + 1 < pts.length; i++) L += Math.hypot(pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1], pts[i + 1][2] - pts[i][2]);
    return L;
  };
  // u in [0,1] -> { p: [x,y,z], dir: unit vector of the segment being walked }
  RR.pointAlong = function (pts, u) {
    if (!pts.length) return { p: [0, 0, 0], dir: [0, 0, 1] };
    if (pts.length === 1) return { p: pts[0].slice(), dir: [0, 0, 1] };
    u = RR.clamp(u, 0, 1);
    const total = RR.polyLen(pts);
    if (total < 1e-9) return { p: pts[0].slice(), dir: [0, 0, 1] };
    let target = u * total;
    for (let i = 0; i + 1 < pts.length; i++) {
      const a = pts[i], b = pts[i + 1];
      const seg = Math.hypot(b[0] - a[0], b[1] - a[1], b[2] - a[2]);
      if (seg < 1e-9) continue;
      if (target <= seg || i + 2 === pts.length) {
        const t = RR.clamp(target / seg, 0, 1);
        return {
          p: [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t],
          dir: [(b[0] - a[0]) / seg, (b[1] - a[1]) / seg, (b[2] - a[2]) / seg]
        };
      }
      target -= seg;
    }
    return { p: pts[pts.length - 1].slice(), dir: [0, 0, 1] };
  };

  // ------------------------------------------------------------------ simulation playback (pure functions of the tick t)
  // Who is moving at tick t? Pickers walk depot -> shelves -> packing during [start, start+ceil(pick)];
  // vehicles drive packing -> customer during [done-ceil(deliv), done] and back during [done, done+ceil(deliv)].
  RR.simActors = function (run, t) {
    const out = [];
    (run.records || []).forEach(r => {
      if (r.status !== 'DELIVERED') return;
      const pickT = Math.ceil(r.pick), delT = Math.ceil(r.deliv);
      if (r.pickPath && r.pickPath.length > 1 && t >= r.start && t <= r.start + pickT) {
        out.push({ key: 'p:' + r.id, kind: 'picker', orderId: r.id, priority: r.priority, path: r.pickPath,
                   u: pickT > 0 ? (t - r.start) / pickT : 1 });
      }
      if (r.delPath && r.delPath.length > 1 && delT > 0) {
        const dispatch = r.done - delT;
        (r.vehicles || '').split('+').filter(Boolean).forEach(v => {
          const parts = v.split(':');
          const base = { kind: 'vehicle', vehicle: parts[0], vehicleId: parts[1] || '', orderId: r.id, priority: r.priority, path: r.delPath };
          if (t >= dispatch && t <= r.done) out.push(Object.assign({ key: 'v:' + r.id + ':' + v, dir: 'out', u: (t - dispatch) / delT }, base));
          else if (t > r.done && t <= r.done + delT) out.push(Object.assign({ key: 'v:' + r.id + ':' + v, dir: 'back', u: 1 - (t - r.done) / delT }, base));
        });
      }
    });
    return out;
  };

  // Bars for the timeline: queue wait, picking, delivery, return trip.
  RR.ganttRows = function (run) {
    return (run.records || []).map(r => {
      const row = { id: r.id, customer: r.customer, priority: r.priority, status: r.status, arrival: r.arrival, start: r.start,
                    late: !!r.late, deadline: r.deadline, done: r.done, vehicles: r.vehicles };
      if (r.status === 'DELIVERED') {
        const delT = Math.ceil(r.deliv);
        row.pickEnd = r.start + Math.ceil(r.pick);
        row.dispatch = r.done - delT;
        row.back = r.done + delT;
      }
      return row;
    });
  };

  RR.makespanOf = run => {
    let m = 0;
    (run.records || []).forEach(r => {
      const delT = r.status === 'DELIVERED' ? Math.ceil(r.deliv) : 0;
      m = Math.max(m, r.done + delT, r.start);
    });
    return m;
  };

  // Chronological list of events with a tick, for the live ticker.
  RR.runEvents = function (run) {
    const ev = [];
    (run.records || []).forEach(r => {
      ev.push({ t: r.arrival, kind: 'arrive', id: r.id, priority: r.priority, text: `${r.id} (${r.customer}) arrives, ${r.priority}` });
      if (r.status === 'DELIVERED') {
        ev.push({ t: r.start, kind: 'pick', id: r.id, priority: r.priority, text: `${r.id} picking starts (${(r.stops || []).length} stop${(r.stops || []).length === 1 ? '' : 's'}, ${RR.fmt1(r.pick)} units)` });
        ev.push({ t: r.done, kind: r.late ? 'late' : 'done', id: r.id, priority: r.priority, text: `${r.id} delivered by ${r.vehicles}${r.late ? ' - LATE' : ''}` });
      } else {
        ev.push({ t: r.start, kind: 'reject', id: r.id, priority: r.priority, text: `${r.id} rejected: ${r.note}` });
      }
    });
    return ev.map((e, i) => Object.assign({ i }, e)).sort((a, b) => a.t - b.t || a.i - b.i);
  };

  // ------------------------------------------------------------------ comparing two runs (ours vs baseline)
  RR.compareVerdict = function (ours, base) {
    const out = [];
    const a = ours.metrics, b = base.metrics;
    const pct = (x, y) => (y === 0 ? null : Math.round(((y - x) / y) * 100));      // positive = ours is lower
    RR.PRIORITIES.forEach(p => {
      const x = a.wait[p].avg, y = b.wait[p].avg;
      if (x === y) { out.push({ tone: 'info', text: `${p} wait is the same (${RR.fmt1(x)} ticks).` }); return; }
      const change = pct(x, y);
      if (x < y) out.push({ tone: 'good', text: `${p} orders wait ${change === null ? 'less' : change + '% less'} (${RR.fmt1(y)} to ${RR.fmt1(x)} ticks).` });
      else out.push({ tone: 'bad', text: `${p} orders wait ${change === null ? 'more' : Math.abs(change) + '% more'} (${RR.fmt1(y)} to ${RR.fmt1(x)} ticks).` });
    });
    if (a.late !== b.late) out.push({ tone: a.late < b.late ? 'good' : 'bad', text: `Late deliveries: ${b.late} to ${a.late}.` });
    if (a.makespan !== b.makespan) out.push({ tone: a.makespan < b.makespan ? 'good' : 'bad', text: `Finish time (makespan): ${b.makespan} to ${a.makespan} ticks.` });
    if (a.avgPick !== b.avgPick) out.push({ tone: a.avgPick < b.avgPick ? 'good' : 'bad', text: `Average picking distance: ${RR.fmt1(b.avgPick)} to ${RR.fmt1(a.avgPick)}.` });
    return out;
  };

  // ------------------------------------------------------------------ shared state, events, API
  RR.store = { state: null, run: null, compare: null, feed: [], demo: false };
  const handlers = {};
  RR.bus = {
    on(ev, fn) { (handlers[ev] = handlers[ev] || []).push(fn); return () => { handlers[ev] = (handlers[ev] || []).filter(f => f !== fn); }; },
    emit(ev, data) { (handlers[ev] || []).slice().forEach(fn => { try { fn(data); } catch (e) { console.error(e); } }); }
  };
  RR.log = function (text, kind) {
    RR.store.feed.unshift({ text, kind: kind || 'info', at: new Date() });
    if (RR.store.feed.length > 40) RR.store.feed.pop();
    RR.bus.emit('feed');
  };

  RR.api = async function (path, method, params) {
    method = method || 'GET';
    if (root.RR_DEMO) return RR.demoApi(path, method, params);        // optional offline snapshot (test builds only)
    const opt = { method };
    if (method === 'POST') {
      opt.headers = { 'Content-Type': 'application/x-www-form-urlencoded' };
      opt.body = new URLSearchParams(params || {}).toString();
    }
    let res;
    try { res = await fetch(path, opt); } catch (e) { throw new Error('Cannot reach the RouteRush server. Is it still running?'); }
    let data = null;
    try { data = await res.json(); } catch (e) { /* not json */ }
    if (!res.ok) throw new Error((data && data.error) || ('HTTP ' + res.status));
    return data;
  };
  RR.demoApi = async function (path, method, params) {
    const D = root.RR_DEMO;
    RR.store.demo = true;
    if (path === '/api/state') return D.state;
    if (path === '/api/compare') return D.compare;
    if (path === '/api/run') return D.runs[((params && (params.fifo === '1' || params.fifo === 'true')) ? '1' : '0') + '|' + ((params && params.pick) || '2opt')];
    throw new Error('Offline demo: start the RouteRush server to change data.');
  };

  RR.loadState = async function () {
    RR.store.state = await RR.api('/api/state');
    RR.bus.emit('state', RR.store.state);
    return RR.store.state;
  };

  // ------------------------------------------------------------------ derived views of the state
  RR.nodeType = function (state, id) {
    if (id === state.depot) return 'depot';
    if (id === state.packing) return 'packing';
    return state.products.some(p => p.node === id) ? 'shelf' : 'city';
  };
  RR.demand = function (state) {                                     // units wanted by all current orders, per product
    const d = {};
    state.orders.forEach(o => o.lines.forEach(l => { d[l.product] = (d[l.product] || 0) + l.qty; }));
    return d;
  };
  // Shortest path between two nodes by edge weight (same idea as the C++ Dijkstra), for the Map page.
  RR.shortestPath = function (state, src, dst) {
    const n = state.nodes.length;
    if (src < 0 || dst < 0 || src >= n || dst >= n) return null;
    const dist = new Array(n).fill(Infinity), prev = new Array(n).fill(-1), done = new Array(n).fill(false);
    dist[src] = 0;
    for (let it = 0; it < n; it++) {
      let u = -1;
      for (let i = 0; i < n; i++) if (!done[i] && (u < 0 || dist[i] < dist[u])) u = i;
      if (u < 0 || dist[u] === Infinity) break;
      done[u] = true;
      state.edges.forEach(e => {
        const v = e.a === u ? e.b : (e.b === u ? e.a : -1);
        if (v >= 0 && dist[u] + e.w < dist[v]) { dist[v] = dist[u] + e.w; prev[v] = u; }
      });
    }
    if (dist[dst] === Infinity) return null;
    const path = [];
    for (let at = dst; at !== -1; at = prev[at]) path.push(at);
    return { dist: dist[dst], path: path.reverse() };
  };

  RR.neighbours = function (state, id) {
    const out = [];
    state.edges.forEach(e => {
      if (e.a === id) out.push({ id: e.b, w: e.w });
      else if (e.b === id) out.push({ id: e.a, w: e.w });
    });
    return out;
  };

  if (typeof module !== 'undefined' && module.exports) module.exports = RR;      // for the Node tests
})(typeof window !== 'undefined' ? window : globalThis);
