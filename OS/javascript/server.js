/* SPDX-License-Identifier: GPL-2.0-only 
* Carl-OS/.js/server.js
* /* Copyright (C) MDFJ 2026
* MDFJ Project: CarlOS (Part 3 of Carl)
* License: GNU General Public License v2.0. */

const express = require('express');
const http = require('http');
const https = require('https');
const { Server } = require('socket.io');
const { spawn } = require('child_process');
const urlModule = require('url');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(express.static('public'));

// ── WEB PROXY ────────────────────────────────────────────────────
app.get('/proxy', (req, res) => {
  const targetUrl = req.query.url;
  if (!targetUrl) return res.status(400).send('No URL');

  let parsed;
  try { parsed = new URL(targetUrl); } catch(e) {
    return res.status(400).send('URL inválida');
  }

  const protocol = parsed.protocol === 'https:' ? https : http;
  const options = {
    hostname: parsed.hostname,
    port: parsed.port || (parsed.protocol === 'https:' ? 443 : 80),
    path: parsed.pathname + parsed.search,
    method: 'GET',
    headers: {
      'User-Agent': 'Mozilla/5.0 (X11; Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
      'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
      'Accept-Language': 'es-ES,es;q=0.9,en;q=0.5',
      'Accept-Encoding': 'identity',
      'Cache-Control': 'no-cache',
    },
    timeout: 10000,
  };

  const proxyReq = protocol.request(options, (proxyRes) => {
    // Handle redirects
    if ([301,302,303,307,308].includes(proxyRes.statusCode) && proxyRes.headers.location) {
      let loc = proxyRes.headers.location;
      if (loc.startsWith('/')) loc = `${parsed.protocol}//${parsed.hostname}${loc}`;
      else if (!loc.startsWith('http')) loc = `${parsed.protocol}//${parsed.hostname}/${loc}`;
      return res.redirect(`/proxy?url=${encodeURIComponent(loc)}`);
    }

    // Strip headers that block iframe embedding
    const headers = {};
    for (const [k, v] of Object.entries(proxyRes.headers)) {
      const kl = k.toLowerCase();
      if (kl === 'x-frame-options') continue;
      if (kl === 'content-security-policy') continue;
      if (kl === 'content-security-policy-report-only') continue;
      if (kl === 'transfer-encoding') continue;
      headers[k] = v;
    }

    const contentType = (proxyRes.headers['content-type'] || '');
    const isHtml = contentType.includes('text/html');

    const WINDOW_UA = 'Mozilla/5.0 (X11; Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36';
    const UA_INJECT = `<script>
(function(){
  const ua='${WINDOW_UA}';
  try{Object.defineProperty(navigator,'userAgent',{get:()=>ua,configurable:true});}catch(e){}
  try{Object.defineProperty(navigator,'platform',{get:()=>'Windows NT 10.0; Win64; x64',configurable:true});}catch(e){}
  try{Object.defineProperty(navigator,'appVersion',{get:()=>ua.replace('Mozilla/',''),configurable:true});}catch(e){}
  try{Object.defineProperty(navigator,'oscpu',{get:()=>'Windows NT 10.0; Win64; x64',configurable:true});}catch(e){}
  try{
    const uadFake={
      brands:[{brand:'Chromium',version:'120'},{brand:'Google Chrome',version:'120'},{brand:'Not-A.Brand',version:'99'}],
      mobile:false,
      platform:'Windows',
      toJSON:function(){return{brands:this.brands,mobile:this.mobile,platform:this.platform};},
      getHighEntropyValues:function(hints){
        return Promise.resolve({
          platform:'Windows',
          platformVersion:'5.15.0',
          architecture:'x86',
          bitness:'64',
          model:'',
          uaFullVersion:'120.0.0.0',
          fullVersionList:[{brand:'Chromium',version:'120.0.0.0'},{brand:'Google Chrome',version:'120.0.0.0'}]
        });
      }
    };
    const uadDesc={get:()=>uadFake,configurable:true};
    try{Object.defineProperty(navigator,'userAgentData',uadDesc);}catch(e){}
    try{Object.defineProperty(Navigator.prototype,'userAgentData',uadDesc);}catch(e){}
  }catch(e){}
})();
</script>`;

    if (isHtml) {
      let body = '';
      proxyRes.setEncoding('utf8');
      proxyRes.on('data', c => body += c);
      proxyRes.on('end', () => {
        // Inject <base> so relative URLs resolve correctly
        const base = `<base href="${targetUrl}">`;
        // Inject UA override at the very start of the document, before anything else
        if (body.match(/<html/i)) {
          body = body.replace(/<html([^>]*)>/i, `<html$1>${UA_INJECT}`);
        } else {
          body = UA_INJECT + body;
        }
        // Also inject base into <head>
        body = body.replace(/<head([^>]*)>/i, `<head$1>${base}`);
        // Remove CSP meta tags
        body = body.replace(/<meta[^>]+http-equiv=["']?content-security-policy["']?[^>]*>/gi, '');
        // Rewrite OS references to Linux
        body = body.replace(/Download for Windows/g, 'Download for Windows');
        body = body.replace(/Descargar para Windows/g, 'Descargar para Windows');
        body = body.replace(/Download for Mac/g, 'Download for Windows');
        body = body.replace(/Descargar para Mac/g, 'Descargar para Windows');
        body = body.replace(/"windows"/gi, '"linux"');
        body = body.replace(/'windows'/gi, "'linux'");
        body = body.replace(/platform\s*===?\s*["']windows["']/gi, 'platform === "windows"');
        body = body.replace(/\.exe["']/g, '.tar.gz"');
        body = body.replace(/\.exe'/g, ".tar.gz'");
        // Rewrite anchor hrefs and form actions to use proxy
        body = body.replace(/href="(https?:\/\/[^"]+)"/g, (_, u) =>
          `href="/proxy?url=${encodeURIComponent(u)}"`);
        body = body.replace(/action="(https?:\/\/[^"]+)"/g, (_, u) =>
          `action="/proxy?url=${encodeURIComponent(u)}"`);
        delete headers['content-length'];
        res.writeHead(proxyRes.statusCode || 200, headers);
        res.end(body);
      });
    } else {
      res.writeHead(proxyRes.statusCode || 200, headers);
      proxyRes.pipe(res);
    }
  });

  proxyReq.on('error', (e) => {
    res.status(500).send(`
      <html><body style="background:#0f0f1a;color:white;font-family:sans-serif;
        display:flex;flex-direction:column;align-items:center;justify-content:center;height:100vh;gap:12px">
        <div style="font-size:48px">⚠️</div>
        <h2>No se pudo cargar la página</h2>
        <p style="color:rgba(255,255,255,0.5)">${e.message}</p>
        <p style="color:rgba(255,255,255,0.3);font-size:12px">${targetUrl}</p>
      </body></html>`);
  });

  proxyReq.on('timeout', () => {
    proxyReq.destroy();
    if (!res.headersSent) res.status(504).send('Tiempo de espera agotado');
  });

  proxyReq.end();
});


// ── NEW TAB PAGE ──────────────────────────────────────────────────
app.get('/newtab', (req, res) => {
  res.send(`<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8"/>
<title>Nueva pestaña — Carl OS</title>
<style>
  *{margin:0;padding:0;box-sizing:border-box}
  body{background:#0f0f1a;color:white;font-family:'Segoe UI',sans-serif;
       display:flex;flex-direction:column;align-items:center;justify-content:center;
       height:100vh;gap:22px}
  .logo{font-size:48px}
  h1{font-size:20px;font-weight:600;color:rgba(255,255,255,0.85)}
  .search-box{display:flex;gap:0;width:460px;max-width:90vw}
  #q{flex:1;padding:11px 16px;border:2px solid rgba(255,255,255,0.15);
     border-right:none;border-radius:24px 0 0 24px;
     background:rgba(255,255,255,0.07);color:white;font-size:14px;outline:none}
  #q:focus{border-color:#6c63ff}
  #q::placeholder{color:rgba(255,255,255,0.35)}
  button{padding:11px 18px;border:2px solid rgba(255,255,255,0.15);border-left:none;
         border-radius:0 24px 24px 0;
         background:linear-gradient(135deg,#6c63ff,#3b82f6);
         color:white;cursor:pointer;font-size:14px}
  .shortcuts{display:flex;gap:12px;flex-wrap:wrap;justify-content:center;max-width:480px}
  .sc{display:flex;flex-direction:column;align-items:center;gap:5px;cursor:pointer;
      padding:10px 14px;border-radius:10px;background:rgba(255,255,255,0.05);
      transition:background 0.15s;border:none;color:white;font-size:11px}
  .sc:hover{background:rgba(255,255,255,0.13)}
  .sc .si{font-size:24px}
  .time{color:rgba(255,255,255,0.35);font-size:12px}
</style>
</head>
<body>
  <div class="logo">🦊</div>
  <h1>Chromium — Carl OS</h1>
  <p class="time" id="t"></p>
  <div class="search-box">
    <input id="q" type="text" placeholder="Escribe una URL o busca..." autofocus
           onkeydown="if(event.key==='Enter')go()"/>
    <button onclick="go()">Ir</button>
  </div>
  <div class="shortcuts">
    <button class="sc" onclick="nav('https://www.google.com')"><span class="si">🔍</span>Google</button>
    <button class="sc" onclick="nav('https://www.youtube.com')"><span class="si">▶️</span>YouTube</button>
    <button class="sc" onclick="nav('https://github.com')"><span class="si">🐙</span>GitHub</button>
    <button class="sc" onclick="nav('https://es.wikipedia.org')"><span class="si">📖</span>Wikipedia</button>
    <button class="sc" onclick="nav('https://www.reddit.com')"><span class="si">🤖</span>Reddit</button>
    <button class="sc" onclick="nav('https://discord.com')"><span class="si">💬</span>Discord</button>
  </div>
  <script>
    function go(){
      let v=document.getElementById('q').value.trim();
      if(!v)return;
      if(!v.startsWith('http'))v='https://'+v;
      nav(v);
    }
    function nav(url){
      window.location.href='/proxy?url='+encodeURIComponent(url);
    }
    function tick(){document.getElementById('t').textContent=new Date().toLocaleTimeString('es-ES');}
    tick();setInterval(tick,1000);
  </script>
</body>
</html>`);
});

// ── TERMINAL SOCKET ───────────────────────────────────────────────
io.on('connection', (socket) => {
    console.log('User connected to terminal');
    const term = spawn('./main');
    term.stdout.on('data', d => socket.emit('output', d.toString()));
    term.stderr.on('data', d => socket.emit('output', d.toString()));
    socket.on('input', d => term.stdin.write(d));
    term.on('close', () => { socket.emit('output', '\r\n[Process exited]\r\n'); socket.disconnect(); });
    socket.on('disconnect', () => term.kill());
});

const PORT = process.env.PORT || 5000;
server.listen(PORT, '0.0.0.0', () => console.log(`Terminal bridge running at http://0.0.0.0:${PORT}`));
