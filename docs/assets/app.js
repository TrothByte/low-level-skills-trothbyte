/* Low-level skills TrothByte — landing interactivity */
"use strict";

const prefersReduced = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

/* ---------- 1. Particle field ---------- */
(function particles() {
  if (prefersReduced) return;
  const canvas = document.getElementById("bg");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  const DPR = Math.min(window.devicePixelRatio || 1, 2);
  let W, H, particles = [], mouse = { x: -1e4, y: -1e4 }, raf = 0;

  const COLORS = ["56,189,248", "94,234,212", "129,230,217"];

  function resize() {
    W = window.innerWidth;
    H = window.innerHeight;
    canvas.width = W * DPR;
    canvas.height = H * DPR;
    ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
    const n = Math.min(90, Math.floor((W * H) / 18000));
    particles = Array.from({ length: n }, () => ({
      x: Math.random() * W,
      y: Math.random() * H,
      vx: (Math.random() - 0.5) * 0.4,
      vy: (Math.random() - 0.5) * 0.4,
      r: Math.random() * 1.6 + 0.6,
      c: COLORS[Math.floor(Math.random() * COLORS.length)],
    }));
  }

  function frame() {
    ctx.clearRect(0, 0, W, H);
    for (const p of particles) {
      p.x += p.vx;
      p.y += p.vy;
      if (p.x < 0 || p.x > W) p.vx *= -1;
      if (p.y < 0 || p.y > H) p.vy *= -1;
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.r, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${p.c},0.5)`;
      ctx.fill();
    }
    // link close particles
    for (let i = 0; i < particles.length; i++) {
      for (let j = i + 1; j < particles.length; j++) {
        const a = particles[i], b = particles[j];
        const dx = a.x - b.x, dy = a.y - b.y;
        const d2 = dx * dx + dy * dy;
        if (d2 < 120 * 120) {
          const alpha = 0.14 * (1 - Math.sqrt(d2) / 120);
          ctx.strokeStyle = `rgba(120,160,220,${alpha})`;
          ctx.lineWidth = 1;
          ctx.beginPath();
          ctx.moveTo(a.x, a.y);
          ctx.lineTo(b.x, b.y);
          ctx.stroke();
        }
      }
      const p = particles[i];
      const mx = mouse.x - p.x, my = mouse.y - p.y;
      const md2 = mx * mx + my * my;
      if (md2 < 160 * 160) {
        const dist = Math.sqrt(md2) || 1;
        ctx.strokeStyle = `rgba(${p.c},${0.35 * (1 - dist / 160)})`;
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(p.x, p.y);
        ctx.lineTo(mouse.x, mouse.y);
        ctx.stroke();
      }
    }
    raf = requestAnimationFrame(frame);
  }

  window.addEventListener("resize", resize);
  window.addEventListener("mousemove", (e) => { mouse.x = e.clientX; mouse.y = e.clientY; });
  window.addEventListener("mouseleave", () => { mouse.x = -1e4; mouse.y = -1e4; });
  document.addEventListener("visibilitychange", () => {
    if (document.hidden) cancelAnimationFrame(raf);
    else raf = requestAnimationFrame(frame);
  });

  resize();
  raf = requestAnimationFrame(frame);
})();

/* ---------- 2. Typing effect ---------- */
(function typewriter() {
  const el = document.getElementById("typed");
  if (!el) return;
  const phrases = [
    "AI agents trust \u201cit compiles\u201d \u2014 Bedrock teaches them to verify.",
    "C, C++, Rust, assembly, kernels, firmware \u2014 124 skills, source-traced.",
    "Assemble \u2192 disassemble \u2192 compare bytes \u2192 then trust.",
    "65 skills executed on real toolchains. 59 honestly marked researched.",
    "Every claim traces to a primary source: claim \u2192 source \u2192 section \u2192 skill.",
  ];
  let pi = 0, ci = 0, deleting = false;

  function tick() {
    const phrase = phrases[pi];
    const text = phrase.slice(0, ci);
    el.textContent = text;
    let delay = deleting ? 24 : 46;
    if (!deleting && ci === phrase.length) delay = 2200;
    if (deleting && ci === 0) {
      deleting = false;
      pi = (pi + 1) % phrases.length;
      delay = 350;
    }
    ci += deleting ? -1 : 1;
    if (ci === phrase.length && !deleting) deleting = true;
    setTimeout(tick, delay);
  }
  tick();
})();

/* ---------- 2b. Terminal demo ---------- */
(function terminal() {
  const body = document.getElementById("terminal-body");
  if (!body) return;
  const lines = [
    ["$ python tools/validate.py", ""],
    ["[validate] checking 124 SKILL.md files", ""],
    ["[validate] skill_lint \u2026\u2026 124/124 OK", "ok"],
    ["[validate] registry_check \u2026 0 warnings", "ok"],
    ["[validate] source_check \u2026 0 warnings", "ok"],
    ["[validate] OK", "ok"],
  ];
  let li = 0, ci = 0;

  if (prefersReduced) {
    body.textContent = lines.map((l) => l[0]).join("\n");
    return;
  }

  function step() {
    if (li >= lines.length) {
      li = 0;
      body.textContent = "";
      setTimeout(step, 3000);
      return;
    }
    const [text, cls] = lines[li];
    if (ci === 0) {
      const line = document.createElement("div");
      if (cls) line.className = cls;
      body.appendChild(line);
    }
    const last = body.lastElementChild;
    last.textContent = text.slice(0, ci + 1);
    ci++;
    if (ci > text.length) {
      ci = 0;
      li++;
    }
    setTimeout(step, cls ? 26 : 44);
  }
  step();
})();

/* ---------- 3. Animated counters ---------- */
(function counters() {
  const stats = document.querySelectorAll("[data-count]");
  if (!stats.length) return;

  const format = (n) => {
    try {
      return new Intl.NumberFormat("en-US").format(n);
    } catch {
      return String(n);
    }
  };

  function run(el) {
    const target = parseInt(el.dataset.count, 10);
    const dur = 1100;
    const t0 = performance.now();
    function step(t) {
      const p = Math.min((t - t0) / dur, 1);
      const eased = 1 - Math.pow(1 - p, 3);
      el.textContent = format(Math.round(target * eased));
      if (p < 1) requestAnimationFrame(step);
      else el.textContent = format(target);
    }
    requestAnimationFrame(step);
  }

  const io = new IntersectionObserver((entries) => {
    for (const e of entries) {
      if (e.isIntersecting) {
        run(e.target);
        io.unobserve(e.target);
      }
    }
  }, { threshold: 0.4 });
  stats.forEach((s) => io.observe(s));
})();

/* ---------- 4. Reveal on scroll ---------- */
(function reveal() {
  const els = document.querySelectorAll(".reveal");
  const io = new IntersectionObserver((entries) => {
    for (const e of entries) {
      if (e.isIntersecting) {
        e.target.classList.add("in");
        io.unobserve(e.target);
      }
    }
  }, { threshold: 0.12 });
  els.forEach((el) => io.observe(el));
})();

/* ---------- 5. Skills explorer ---------- */
(function explorer() {
  const grid = document.getElementById("skill-grid");
  const search = document.getElementById("search");
  const meta = document.getElementById("result-meta");
  const chips = Array.from(document.querySelectorAll(".chip[data-filter]"));
  if (!grid || !window.SKILLS) return;

  const state = { q: "", stability: "all" };

  function matches(s) {
    if (state.stability !== "all" && s.stability !== state.stability) return false;
    if (!state.q) return true;
    const q = state.q.toLowerCase();
    return (
      s.id.toLowerCase().includes(q) ||
      s.name.toLowerCase().includes(q) ||
      s.domain.toLowerCase().includes(q) ||
      s.desc.toLowerCase().includes(q)
    );
  }

  function render() {
    const list = window.SKILLS.filter(matches);
    const frag = document.createDocumentFragment();

    for (const s of list) {
      const card = document.createElement("a");
      card.className = "skill-card";
      card.href = `https://github.com/TrothByte/low-level-skills-trothbyte/tree/main/skills/${encodeURIComponent(s.domain)}/${encodeURIComponent(s.id)}`;
      card.setAttribute("aria-label", `${s.name} (${s.stability})`);
      const badge = s.stability in { "source-backed": 1, "evaluated": 1, "stable": 1 } ? s.stability : "researched";
      card.innerHTML =
        `<h3>${s.name} <span class="domain-chip">${s.domain}</span></h3>` +
        `<p>${escapeHtml(s.desc)}</p>` +
        `<div class="card-meta"><span class="badge badge-${badge}">${badge}</span><code>${s.id}</code></div>`;
      frag.appendChild(card);
    }

    grid.textContent = "";
    if (!list.length) {
      const empty = document.createElement("div");
      empty.className = "empty";
      empty.textContent = "No skills match your search \u2014 try another term or clear the filter.";
      grid.appendChild(empty);
    } else {
      grid.appendChild(frag);
    }

    if (meta) {
      const label = state.q ? ` \u201cmatching \u201C${state.q}\u201D\u201d` : "";
      meta.textContent = `${list.length} of ${window.SKILLS.length} skills${label}`;
    }
  }

  function escapeHtml(s) {
    const d = document.createElement("div");
    d.textContent = s;
    return d.innerHTML;
  }

  if (search) {
    search.addEventListener("input", () => {
      state.q = search.value.trim();
      render();
    });
  }

  chips.forEach((chip) => {
    chip.addEventListener("click", () => {
      chips.forEach((c) => c.setAttribute("aria-pressed", c === chip ? "true" : "false"));
      state.stability = chip.dataset.filter;
      render();
    });
  });

  render();
})();

/* ---------- 6. Copy buttons ---------- */
(function copyButtons() {
  const live = document.getElementById("copy-live");
  document.querySelectorAll("[data-copy]").forEach((btn) => {
    btn.addEventListener("click", async () => {
      const text = btn.dataset.copy;
      try {
        await navigator.clipboard.writeText(text);
      } catch {
        const ta = document.createElement("textarea");
        ta.value = text;
        document.body.appendChild(ta);
        ta.select();
        try { document.execCommand("copy"); } catch { /* noop */ }
        ta.remove();
      }
      btn.dataset.copied = "true";
      const label = btn.textContent;
      btn.textContent = "Copied \u2713";
      if (live) live.textContent = `Copied: ${text}`;
      setTimeout(() => {
        btn.dataset.copied = "false";
        btn.textContent = label;
      }, 1600);
    });
  });
})();

/* ---------- 7. Install tabs ---------- */
(function tabs() {
  const tabs = Array.from(document.querySelectorAll("[role='tab']"));
  tabs.forEach((tab) => {
    tab.addEventListener("click", () => {
      tabs.forEach((t) => t.setAttribute("aria-selected", t === tab ? "true" : "false"));
      document.querySelectorAll("[role='tabpanel']").forEach((p) => {
        const active = p.id === tab.getAttribute("aria-controls");
        p.hidden = !active;
      });
    });
  });
})();

/* ---------- 8. Active nav on scroll ---------- */
(function nav() {
  const links = Array.from(document.querySelectorAll(".nav-links a[href^='#']"));
  if (!links.length) return;
  const sections = links
    .map((l) => document.querySelector(l.getAttribute("href")))
    .filter(Boolean);
  const io = new IntersectionObserver(
    (entries) => {
      for (const e of entries) {
        if (e.isIntersecting) {
          links.forEach((l) => {
            const active = l.getAttribute("href") === "#" + e.target.id;
            l.style.color = active ? "var(--text)" : "";
          });
        }
      }
    },
    { rootMargin: "-45% 0px -50% 0px" }
  );
  sections.forEach((s) => io.observe(s));
})();
