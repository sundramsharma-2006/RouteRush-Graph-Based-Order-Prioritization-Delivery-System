/* RouteRush web - the 3D warehouse (Three.js r128).
   Nodes are shelves / customer areas / depot / packing, edges are beams, and pickers and vehicles are driven along the
   exact node-by-node routes the C++ backend computed. Orbit with drag, zoom with the wheel, click a node to select it. */
(function (root) {
  'use strict';
  const RR = root.RR = root.RR || {};

  RR.hasWebGL = function () {
    try {
      const c = document.createElement('canvas');
      return !!(root.THREE && root.WebGLRenderingContext && (c.getContext('webgl') || c.getContext('experimental-webgl')));
    } catch (e) { return false; }
  };

  const COLORS = { depot: 0xf1e6d8, packing: 0xe8b04a, shelf: 0x8fae6b, city: 0xc8645a };
  const VEHICLE_COLORS = { Bike: 0x8fae6b, Van: 0xe8b04a, Truck: 0xe0793f };
  const layoutCache = {};

  // ------------------------------------------------------------------ canvas textures
  function glowTexture() {
    const T = root.THREE, c = document.createElement('canvas');
    c.width = c.height = 128;
    const g = c.getContext('2d'), grad = g.createRadialGradient(64, 64, 0, 64, 64, 64);
    grad.addColorStop(0, 'rgba(255,255,255,1)'); grad.addColorStop(0.25, 'rgba(255,255,255,0.45)'); grad.addColorStop(1, 'rgba(255,255,255,0)');
    g.fillStyle = grad; g.fillRect(0, 0, 128, 128);
    const tex = new T.CanvasTexture(c);
    tex.encoding = T.sRGBEncoding;
    return tex;
  }
  function labelTexture(text, sub, color) {
    const T = root.THREE, c = document.createElement('canvas');
    c.width = 512; c.height = 128;
    const g = c.getContext('2d');
    g.textAlign = 'center'; g.textBaseline = 'middle';
    g.font = '700 52px "Rajdhani","Segoe UI",system-ui,sans-serif';
    g.shadowColor = color || '#e0793f'; g.shadowBlur = 0;
    g.fillStyle = '#ffffff';
    g.fillText(text, 256, sub ? 48 : 64);
    if (sub) {
      g.shadowBlur = 0; g.font = '500 30px "Inter","Segoe UI",system-ui,sans-serif'; g.fillStyle = 'rgba(241,230,216,0.9)';
      g.fillText(sub, 256, 96);
    }
    const tex = new T.CanvasTexture(c);
    tex.minFilter = T.LinearFilter;
    tex.encoding = T.sRGBEncoding;
    return tex;
  }

  RR.WarehouseScene = class WarehouseScene {
    constructor(container, opts) {
      const T = root.THREE;
      this.T = T;
      this.container = container;
      this.opts = Object.assign({ interactive: true, autoRotate: true, labels: true, weights: false, pulses: true, onSelect: null, onHover: null }, opts || {});
      this.nodePos = [];
      this.nodeObjs = [];
      this.actors = new Map();
      this.selectedId = null;
      this.hoverId = null;
      this.graph = null;
      this.destroyed = false;
      this.disposables = [];
      this.clock = { last: performance.now(), t: 0 };

      this.scene = new T.Scene();
      this.scene.fog = new T.FogExp2(0x1c1511, 0.011);
      this.camera = new T.PerspectiveCamera(48, 1, 0.1, 400);
      this.renderer = new T.WebGLRenderer({ antialias: true, alpha: true, powerPreference: 'high-performance' });
      this.renderer.setPixelRatio(Math.min(root.devicePixelRatio || 1, 2));
      this.renderer.outputEncoding = T.sRGBEncoding;
      this.renderer.setClearColor(0x000000, 0);
      this.canvas = this.renderer.domElement;
      this.canvas.className = 'scene-canvas';
      container.appendChild(this.canvas);

      this.glowTex = glowTexture();
      this.disposables.push(this.glowTex);

      // lights
      this.scene.add(new T.AmbientLight(0xd9c3a8, 0.75));
      const key = new T.DirectionalLight(0xffffff, 0.8); key.position.set(10, 22, 12); this.scene.add(key);
      this.lightA = new T.PointLight(0xffb070, 1.4, 90); this.lightA.position.set(-14, 12, -6); this.scene.add(this.lightA);
      this.lightB = new T.PointLight(0xe8b04a, 1.0, 80); this.lightB.position.set(14, 9, 10); this.scene.add(this.lightB);

      this.worldGroup = new T.Group(); this.scene.add(this.worldGroup);       // nodes + edges (rebuilt by setGraph)
      this.actorGroup = new T.Group(); this.scene.add(this.actorGroup);
      this.pathGroup = new T.Group(); this.scene.add(this.pathGroup);

      this._buildStars();
      this._buildShared();

      // camera orbit state (goal values are eased towards every frame)
      this.orbit = { theta: 0.7, phi: 1.05, radius: 30, target: new T.Vector3(0, 0, 0) };
      this.goal = { theta: 0.7, phi: 1.05, radius: 30, target: new T.Vector3(0, 0, 0) };
      this.lastInteract = -10000;
      this.dragging = false;
      this.pointer = { x: 0, y: 0, inside: false };
      this.raycaster = new T.Raycaster();
      this.ndc = new T.Vector2();

      this._bind();
      this._resize();
      this._tick = this._tick.bind(this);
      this._raf = requestAnimationFrame(this._tick);
    }

    // ---------------------------------------------------------------- shared geometry/material
    _buildShared() {
      const T = this.T, s = this.shared = {};
      s.box = new T.BoxGeometry(1, 1, 1);
      s.wheel = new T.CylinderGeometry(0.17, 0.17, 0.14, 14); s.wheel.rotateZ(Math.PI / 2);
      s.sphere = new T.SphereGeometry(1, 20, 14);
      s.beam = new T.CylinderGeometry(1, 1, 1, 6, 1, true);
      s.dark = new T.MeshStandardMaterial({ color: 0x2a2018, roughness: 0.8, metalness: 0.2 });
      s.hit = new T.MeshBasicMaterial({ transparent: true, opacity: 0, depthWrite: false });
      this.disposables.push(s.box, s.wheel, s.sphere, s.beam, s.dark, s.hit);
    }

    _buildStars() {
      const T = this.T, n = 260, pos = new Float32Array(n * 3);
      const rnd = RR.mulberry32(99);
      for (let i = 0; i < n; i++) {
        const r = 35 + rnd() * 55, a = rnd() * Math.PI * 2, b = Math.acos(2 * rnd() - 1);
        pos[i * 3] = r * Math.sin(b) * Math.cos(a); pos[i * 3 + 1] = r * Math.cos(b) * 0.7 + 8; pos[i * 3 + 2] = r * Math.sin(b) * Math.sin(a);
      }
      const geo = new T.BufferGeometry();
      geo.setAttribute('position', new T.BufferAttribute(pos, 3));
      const mat = new T.PointsMaterial({ color: 0xd9b890, size: 0.3, transparent: true, opacity: 0.35, depthWrite: false, blending: T.AdditiveBlending, map: null });
      this.stars = new T.Points(geo, mat);
      this.scene.add(this.stars);
      this.disposables.push(geo, mat);
    }

    // ---------------------------------------------------------------- building the world from the graph
    /* graph = { nodes:[{id,name,role,dist}], edges:[{a,b,w}], depot, packing, info:{ [id]: {type, fill} } } */
    setGraph(graph) {
      const T = this.T;
      this.graph = graph;
      this._clearGroup(this.worldGroup);
      this._clearGroup(this.pathGroup);
      this.nodeObjs = [];
      this.pulses = [];
      this.hitMeshes = [];

      const key = graph.nodes.length + ':' + graph.edges.map(e => e.a + '-' + e.b + '-' + e.w).join(',');
      const layout = layoutCache[key] || (layoutCache[key] = RR.springLayout(graph.nodes, graph.edges));
      this.nodePos = layout.map(p => p.slice());
      let minY = 0;
      this.nodePos.forEach(p => { minY = Math.min(minY, p[1]); });
      this.floorY = minY - 1.4;
      const centre = new T.Vector3(0, 0, 0);
      this.nodePos.forEach(p => centre.add(new T.Vector3(p[0], p[1], p[2])));
      if (this.nodePos.length) centre.multiplyScalar(1 / this.nodePos.length);
      let ext = 8;
      this.nodePos.forEach(p => { ext = Math.max(ext, Math.hypot(p[0] - centre.x, p[2] - centre.z)); });
      this.goal.target.copy(centre); this.goal.radius = Math.max(24, ext * 2.35);
      if (!this._viewInit) { this.orbit.target.copy(centre); this.orbit.radius = this.goal.radius * 1.3; this._viewInit = true; }

      this._buildFloor(centre, ext);
      graph.edges.forEach((e, idx) => this._buildEdge(e, idx));
      graph.nodes.forEach(n => this._buildNode(n, graph.info && graph.info[n.id] ? graph.info[n.id] : { type: 'city', fill: 0 }));
      this.setSelected(this.selectedId);
      return this;
    }

    _clearGroup(g) {
      while (g.children.length) {
        const c = g.children[0];
        g.remove(c);
        c.traverse(o => {
          if (o.geometry && !o.isSprite && !Object.values(this.shared).includes(o.geometry)) o.geometry.dispose();
          if (o.material && !Object.values(this.shared).includes(o.material)) {
            if (o.material.map && o.material.map !== this.glowTex) o.material.map.dispose();
            o.material.dispose();
          }
        });
      }
    }

    _p(id, lift) { const p = this.nodePos[id] || [0, 0, 0]; return new this.T.Vector3(p[0], p[1] + (lift || 0), p[2]); }

    _buildFloor(centre, ext) {
      const T = this.T, size = Math.max(60, ext * 4.5);
      const grid = new T.GridHelper(size, Math.round(size / 1.6), 0x5a4332, 0x3a2c20);
      grid.position.set(centre.x, this.floorY, centre.z);
      grid.material.transparent = true; grid.material.opacity = 0.42;
      this.worldGroup.add(grid);
      const glow = new T.Mesh(new T.PlaneGeometry(size * 0.9, size * 0.9),
        new T.MeshBasicMaterial({ map: this.glowTex, color: 0xb4501f, transparent: true, opacity: 0.12, depthWrite: false, blending: T.AdditiveBlending }));
      glow.rotation.x = -Math.PI / 2; glow.position.set(centre.x, this.floorY + 0.02, centre.z);
      this.worldGroup.add(glow);
    }

    _beam(a, b, radius, color, opacity) {
      const T = this.T;
      const dir = new T.Vector3().subVectors(b, a), len = dir.length();
      const m = new T.Mesh(this.shared.beam, new T.MeshBasicMaterial({ color, transparent: true, opacity, depthWrite: false }));
      m.position.copy(a).add(b).multiplyScalar(0.5);
      m.scale.set(radius, Math.max(len, 0.001), radius);
      if (len > 1e-6) m.quaternion.setFromUnitVectors(new T.Vector3(0, 1, 0), dir.normalize());
      return m;
    }

    _sprite(tex, color, scale, opacity, additive) {
      const T = this.T;
      const sp = new T.Sprite(new T.SpriteMaterial({ map: tex, color: color === undefined ? 0xffffff : color, transparent: true, opacity: opacity === undefined ? 1 : opacity,
        depthWrite: false, blending: additive ? T.AdditiveBlending : T.NormalBlending }));
      sp.scale.set(scale[0], scale[1] === undefined ? scale[0] : scale[1], 1);
      return sp;
    }

    _buildEdge(e, idx) {
      const T = this.T, a = this._p(e.a), b = this._p(e.b);
      this.worldGroup.add(this._beam(a, b, 0.05, 0x8a6a4d, 0.5));
      if (this.opts.weights) {
        const tex = labelTexture(String(e.w), '', '#b09a85');
        const sp = this._sprite(tex, 0xffffff, [2.2, 0.55], 0.9, false);
        sp.position.copy(a).add(b).multiplyScalar(0.5); sp.position.y += 0.5; sp.renderOrder = 9;
        sp.material.depthTest = false;
        this.worldGroup.add(sp);
      }
      if (this.opts.pulses) {
        const m = new T.Sprite(new T.SpriteMaterial({ map: this.glowTex, color: idx % 3 === 0 ? 0xe8b04a : 0xffb070, transparent: true, opacity: 0.95, depthWrite: false, blending: T.AdditiveBlending }));
        m.scale.set(0.9, 0.9, 1);
        this.worldGroup.add(m);
        this.pulses.push({ m, a, b, u: (idx * 0.37) % 1, speed: 0.08 + ((idx * 7) % 5) * 0.03, dir: idx % 2 ? 1 : -1 });
      }
    }

    _buildNode(n, info) {
      const T = this.T, type = info.type || 'city', color = COLORS[type] || 0xffffff;
      const g = new T.Group();
      const p = this._p(n.id);
      g.position.copy(p);
      const mat = (c, em) => new T.MeshStandardMaterial({ color: c, emissive: c, emissiveIntensity: em === undefined ? 0.5 : em, metalness: 0.35, roughness: 0.38 });
      const spin = [];

      if (type === 'depot') {
        const cone = new T.Mesh(new T.ConeGeometry(1.15, 2.2, 4), mat(color, 0.55));
        cone.position.y = 1.1; cone.rotation.y = Math.PI / 4; g.add(cone); spin.push({ o: cone, s: 0.25 });
        const base = new T.Mesh(new T.CylinderGeometry(1.7, 1.9, 0.25, 4), mat(color, 0.25)); base.rotation.y = Math.PI / 4; g.add(base);
      } else if (type === 'packing') {
        const oct = new T.Mesh(new T.OctahedronGeometry(1.25), mat(color, 0.65));
        oct.position.y = 1.7; g.add(oct); spin.push({ o: oct, s: 0.7 });
        const ring = new T.Mesh(new T.TorusGeometry(1.9, 0.07, 8, 56), new T.MeshBasicMaterial({ color, transparent: true, opacity: 0.8 }));
        ring.rotation.x = Math.PI / 2; ring.position.y = 0.4; g.add(ring); spin.push({ o: ring, s: -0.5, axis: 'z' });
        const base = new T.Mesh(new T.CylinderGeometry(1.6, 1.8, 0.25, 24), mat(color, 0.25)); g.add(base);
      } else if (type === 'shelf') {
        const fill = RR.clamp(info.fill === undefined ? 0.5 : info.fill, 0, 1);
        for (let i = 0; i < 3; i++) {
          const filled = fill > i / 3 + 0.001;
          const box = new T.Mesh(this.shared.box, filled ? mat(color, 0.5) : new T.MeshBasicMaterial({ color: 0x6b5240, wireframe: true, transparent: true, opacity: 0.55 }));
          box.scale.set(2.1, 0.5, 1.35); box.position.y = 0.3 + i * 0.58;
          g.add(box);
        }
        const base = new T.Mesh(new T.BoxGeometry(2.5, 0.12, 1.7), mat(color, 0.2)); base.position.y = 0.05; g.add(base);
      } else {
        const dome = new T.Mesh(new T.SphereGeometry(0.95, 24, 16), new T.MeshStandardMaterial({ color, emissive: color, emissiveIntensity: 0.55, metalness: 0.3, roughness: 0.3, transparent: true, opacity: 0.88 }));
        dome.position.y = 1.0; g.add(dome);
        const wire = new T.Mesh(new T.SphereGeometry(1.12, 14, 10), new T.MeshBasicMaterial({ color, wireframe: true, transparent: true, opacity: 0.35 }));
        wire.position.y = 1.0; g.add(wire); spin.push({ o: wire, s: 0.4 });
        const ring = new T.Mesh(new T.TorusGeometry(1.7, 0.05, 8, 48), new T.MeshBasicMaterial({ color, transparent: true, opacity: 0.7 }));
        ring.rotation.x = Math.PI / 2; ring.position.y = 0.15; g.add(ring);
      }

      const halo = this._sprite(this.glowTex, color, [type === 'shelf' ? 6 : 6.5], 0.55, true);
      halo.position.y = 1.2; g.add(halo);

      let label = null;
      if (this.opts.labels) {
        const sub = type === 'depot' ? 'depot' : type === 'packing' ? 'packing' : n.dist === null ? '' : (type === 'city' ? 'customer ' : 'shelf ') + RR.fmt1(n.dist) + ' away';
        label = this._sprite(labelTexture(n.name, sub, '#' + color.toString(16).padStart(6, '0')), 0xffffff, [5.2, 1.3], 1, false);
        label.position.y = 3.5; label.renderOrder = 10; label.material.depthTest = false;
        g.add(label);
      }
      const hit = new T.Mesh(this.shared.sphere, this.shared.hit);
      hit.scale.setScalar(1.9); hit.position.y = 1.1; hit.userData.nodeId = n.id;
      g.add(hit); this.hitMeshes.push(hit);

      this.worldGroup.add(g);
      this.nodeObjs[n.id] = { g, halo, label, spin, base: g.position.y, phase: n.id * 0.9, type, color };
    }

    // ---------------------------------------------------------------- public controls
    setSelected(id) {
      const T = this.T;
      this.selectedId = id === undefined ? null : id;
      if (this.selRing) { this.worldGroup.remove(this.selRing); this.selRing.geometry.dispose(); this.selRing.material.dispose(); this.selRing = null; }
      if (this.selectedId === null || !this.nodePos[this.selectedId]) return;
      const ring = new T.Mesh(new T.TorusGeometry(2.3, 0.09, 8, 56), new T.MeshBasicMaterial({ color: 0xffb070, transparent: true, opacity: 0.95 }));
      ring.rotation.x = Math.PI / 2;
      ring.position.copy(this._p(this.selectedId, 0.2));
      this.worldGroup.add(ring);
      this.selRing = ring;
    }
    focusNode(id) { if (this.nodePos[id]) { this.goal.target.copy(this._p(id, 0.8)); this.goal.radius = Math.max(14, this.goal.radius * 0.7); this.lastInteract = performance.now(); } }
    resetView() {
      const c = new this.T.Vector3();
      this.nodePos.forEach(p => c.add(new this.T.Vector3(p[0], p[1], p[2])));
      if (this.nodePos.length) c.multiplyScalar(1 / this.nodePos.length);
      let ext = 8;
      this.nodePos.forEach(p => { ext = Math.max(ext, Math.hypot(p[0] - c.x, p[2] - c.z)); });
      this.goal.target.copy(c); this.goal.radius = Math.max(24, ext * 2.35); this.goal.theta = 0.7; this.goal.phi = 1.05;
    }
    setOption(name, value) { this.opts[name] = value; if (this.graph && (name === 'labels' || name === 'weights' || name === 'pulses')) this.setGraph(this.graph); }

    highlightPath(ids, color) {
      this._clearGroup(this.pathGroup);
      if (!ids || ids.length < 2) return;
      for (let i = 0; i + 1 < ids.length; i++) this.pathGroup.add(this._beam(this._p(ids[i], 0.15), this._p(ids[i + 1], 0.15), 0.13, color || 0xe8b04a, 0.95));
    }

    // list = [{ key, kind:'picker'|'vehicle', vehicle:'Bike'|'Van'|'Truck', vehicleId, orderId, path:[nodeIds], u:0..1, dir:'out'|'back' }]
    updateActors(list) {
      const seen = new Set();
      list.forEach(a => {
        if (!a.path || a.path.length < 2) return;
        seen.add(a.key);
        let o = this.actors.get(a.key);
        if (!o) { o = this._makeActor(a); this.actors.set(a.key, o); this.actorGroup.add(o); }
        const pts = a.path.map(id => { const p = this.nodePos[id] || [0, 0, 0]; return [p[0], p[1] + (a.kind === 'picker' ? 0.9 : 0.5), p[2]]; });
        const r = RR.pointAlong(pts, a.u);
        o.position.set(r.p[0], r.p[1], r.p[2]);
        const f = a.dir === 'back' ? -1 : 1;
        if (a.kind === 'vehicle') o.lookAt(r.p[0] + r.dir[0] * f, r.p[1] + r.dir[1] * f, r.p[2] + r.dir[2] * f);
      });
      this.actors.forEach((o, k) => { if (!seen.has(k)) { this.actorGroup.remove(o); this._disposeActor(o); this.actors.delete(k); } });
    }
    clearActors() { this.updateActors([]); }

    _makeActor(a) {
      const T = this.T, g = new T.Group();
      const color = a.kind === 'picker' ? 0xffffff : (VEHICLE_COLORS[a.vehicle] || 0xffffff);
      const mat = new T.MeshStandardMaterial({ color, emissive: color, emissiveIntensity: 0.55, metalness: 0.3, roughness: 0.35 });
      const add = (sx, sy, sz, x, y, z, m) => { const b = new T.Mesh(this.shared.box, m || mat); b.scale.set(sx, sy, sz); b.position.set(x, y, z); g.add(b); return b; };
      const wheels = (n, zs, x) => zs.forEach(z => { [-1, 1].forEach(sx => { const w = new T.Mesh(this.shared.wheel, this.shared.dark); w.position.set(sx * x, -0.02, z); g.add(w); }); });
      const cab = new T.MeshStandardMaterial({ color: 0x3a2c20, emissive: color, emissiveIntensity: 0.15, metalness: 0.4, roughness: 0.4 });
      if (a.kind === 'picker') {
        const s = new T.Mesh(this.shared.sphere, mat); s.scale.setScalar(0.32); g.add(s);
        const ring = new T.Mesh(new T.TorusGeometry(0.5, 0.04, 6, 24), new T.MeshBasicMaterial({ color, transparent: true, opacity: 0.9 })); ring.rotation.x = Math.PI / 2; g.add(ring);
      } else if (a.vehicle === 'Truck') {
        add(1.15, 0.85, 2.3, 0, 0.42, -0.2); add(1.15, 0.55, 0.75, 0, 0.27, 1.45, cab); wheels(3, [-1.0, 0.1, 1.45], 0.6);
      } else if (a.vehicle === 'Van') {
        add(0.95, 0.62, 1.45, 0, 0.31, -0.1); add(0.95, 0.42, 0.5, 0, 0.21, 0.9, cab); wheels(2, [-0.6, 0.85], 0.5);
      } else {
        add(0.42, 0.26, 0.95, 0, 0.15, 0); add(0.2, 0.2, 0.3, 0, 0.22, 0.55, cab); wheels(1, [-0.35, 0.45], 0.0);
      }
      const halo = this._sprite(this.glowTex, color, [2.6], 0.65, true); halo.position.y = 0.4; g.add(halo);
      const tag = this._sprite(labelTexture(a.orderId + (a.kind === 'vehicle' ? ' ' + (a.vehicleId || '') : ''), '', '#' + color.toString(16).padStart(6, '0')), 0xffffff, [2.4, 0.6], 1, false);
      tag.position.y = a.kind === 'picker' ? 1.1 : 1.5; tag.renderOrder = 11; tag.material.depthTest = false;
      g.add(tag);
      g.userData.own = [mat, cab, tag.material, tag.material.map, halo.material];
      return g;
    }
    _disposeActor(o) {
      (o.userData.own || []).forEach(x => { if (x && x.dispose) x.dispose(); });
      o.traverse(c => { if (c.geometry && !Object.values(this.shared).includes(c.geometry)) c.geometry.dispose(); if (c.material && c.material.type === 'MeshBasicMaterial' && c.material !== this.shared.hit) c.material.dispose(); });
    }

    // ---------------------------------------------------------------- input
    _bind() {
      const c = this.canvas;
      this._on = (el, ev, fn, opt) => { el.addEventListener(ev, fn, opt); (this._unbind = this._unbind || []).push(() => el.removeEventListener(ev, fn, opt)); };
      if (this.opts.interactive) {
        let lx = 0, ly = 0, moved = 0;
        this._on(c, 'pointerdown', e => { this.dragging = true; moved = 0; lx = e.clientX; ly = e.clientY; try { c.setPointerCapture(e.pointerId); } catch (x) { /* ignore */ } this.lastInteract = performance.now(); });
        this._on(c, 'pointermove', e => {
          const r = c.getBoundingClientRect();
          this.pointer.x = e.clientX - r.left; this.pointer.y = e.clientY - r.top; this.pointer.inside = true;
          if (!this.dragging) return;
          const dx = e.clientX - lx, dy = e.clientY - ly; lx = e.clientX; ly = e.clientY; moved += Math.abs(dx) + Math.abs(dy);
          this.goal.theta -= dx * 0.006; this.goal.phi = RR.clamp(this.goal.phi - dy * 0.005, 0.18, 1.5);
          this.lastInteract = performance.now();
        });
        this._on(c, 'pointerup', e => {
          this.dragging = false;
          try { c.releasePointerCapture(e.pointerId); } catch (x) { /* ignore */ }
          if (moved < 6) {
            const id = this._pick(e.clientX, e.clientY);
            this.setSelected(id === null ? null : id);
            if (this.opts.onSelect) this.opts.onSelect(id);
          }
        });
        this._on(c, 'pointerleave', () => { this.pointer.inside = false; });
        this._on(c, 'wheel', e => { e.preventDefault(); this.goal.radius = RR.clamp(this.goal.radius * (1 + e.deltaY * 0.0012), 9, 90); this.lastInteract = performance.now(); }, { passive: false });
        this._on(c, 'dblclick', () => this.resetView());
      }
      if (root.ResizeObserver) { this._ro = new ResizeObserver(() => this._resize()); this._ro.observe(this.container); }
      else this._on(root, 'resize', () => this._resize());
    }

    _pick(clientX, clientY) {
      const r = this.canvas.getBoundingClientRect();
      this.ndc.set(((clientX - r.left) / r.width) * 2 - 1, -((clientY - r.top) / r.height) * 2 + 1);
      this.raycaster.setFromCamera(this.ndc, this.camera);
      const hits = this.raycaster.intersectObjects(this.hitMeshes || [], false);
      return hits.length ? hits[0].object.userData.nodeId : null;
    }

    _resize() {
      if (this.destroyed) return;
      const w = Math.max(1, this.container.clientWidth), h = Math.max(1, this.container.clientHeight);
      this.renderer.setSize(w, h, false);
      this.canvas.style.width = '100%'; this.canvas.style.height = '100%';
      this.camera.aspect = w / h; this.camera.updateProjectionMatrix();
    }

    // ---------------------------------------------------------------- the render loop
    _tick(now) {
      if (this.destroyed) return;
      if (!this.container.isConnected) { this.destroy(); return; }
      this._raf = requestAnimationFrame(this._tick);
      const dt = Math.min(0.05, (now - this.clock.last) / 1000); this.clock.last = now; this.clock.t += dt;
      const t = this.clock.t, reduce = RR.reducedMotion && RR.reducedMotion();

      if (this.opts.autoRotate && !this.dragging && now - this.lastInteract > 2500 && !reduce) this.goal.theta += dt * 0.11;
      const k = 1 - Math.pow(0.0006, dt);                                    // frame-rate independent easing
      const o = this.orbit, g = this.goal;
      o.theta += (g.theta - o.theta) * k; o.phi += (g.phi - o.phi) * k; o.radius += (g.radius - o.radius) * k;
      o.target.lerp(g.target, k);
      const sp = Math.sin(o.phi);
      this.camera.position.set(o.target.x + o.radius * sp * Math.sin(o.theta), o.target.y + o.radius * Math.cos(o.phi), o.target.z + o.radius * sp * Math.cos(o.theta));
      this.camera.lookAt(o.target);

      this.nodeObjs.forEach(n => {
        if (!n) return;
        n.spin.forEach(s => { if (s.axis === 'z') s.o.rotation.z += s.s * dt; else s.o.rotation.y += s.s * dt; });
        n.halo.material.opacity = 0.42 + 0.18 * Math.sin(t * 1.8 + n.phase);
        if (n.type === 'packing' || n.type === 'depot') n.g.position.y = n.base + Math.sin(t * 1.6 + n.phase) * 0.12;
      });
      (this.pulses || []).forEach(p => {
        p.u += p.speed * dt * p.dir; if (p.u > 1) p.u -= 1; else if (p.u < 0) p.u += 1;
        p.m.position.lerpVectors(p.a, p.b, p.u);
      });
      if (this.selRing) { const s = 1 + 0.08 * Math.sin(t * 4); this.selRing.scale.set(s, s, s); this.selRing.rotation.z += dt * 0.8; }
      if (this.stars) this.stars.rotation.y += dt * 0.012;
      this.lightA.position.x = -14 + Math.sin(t * 0.4) * 5; this.lightB.position.z = 10 + Math.cos(t * 0.35) * 5;

      if (this.opts.interactive && this.pointer.inside && !this.dragging) {            // hover cursor
        const r = this.canvas.getBoundingClientRect();
        const id = this._pick(r.left + this.pointer.x, r.top + this.pointer.y);
        if (id !== this.hoverId) { this.hoverId = id; this.canvas.style.cursor = id === null ? 'grab' : 'pointer'; if (this.opts.onHover) this.opts.onHover(id); }
      }
      this.renderer.render(this.scene, this.camera);
    }

    destroy() {
      if (this.destroyed) return;
      this.destroyed = true;
      cancelAnimationFrame(this._raf);
      (this._unbind || []).forEach(f => f());
      if (this._ro) this._ro.disconnect();
      this.actors.forEach(o => this._disposeActor(o)); this.actors.clear();
      this._clearGroup(this.worldGroup); this._clearGroup(this.pathGroup); this._clearGroup(this.actorGroup);
      this.disposables.forEach(d => { try { d.dispose(); } catch (e) { /* ignore */ } });
      this.renderer.dispose();
      try { this.renderer.forceContextLoss(); } catch (e) { /* ignore */ }
      if (this.canvas.parentNode) this.canvas.parentNode.removeChild(this.canvas);
    }
  };

  // Build the graph description the scene needs from the API state.
  RR.sceneGraph = function (state) {
    const info = {};
    const qty = {};
    state.products.forEach(p => { qty[p.node] = (qty[p.node] || 0) + p.qty; });
    const maxQty = Math.max(1, ...Object.values(qty));
    state.nodes.forEach(n => {
      const type = RR.nodeType(state, n.id);
      info[n.id] = { type, fill: type === 'shelf' ? Math.min(1, (qty[n.id] || 0) / maxQty) : 0 };
    });
    return { nodes: state.nodes, edges: state.edges, depot: state.depot, packing: state.packing, info };
  };
})(typeof window !== 'undefined' ? window : globalThis);
