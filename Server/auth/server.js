// Simple Auth/OAuth + Session service for spetals.io
// - Uses Discord OAuth to identify user
// - Creates/links a Game Account (UUID v4) in SQLite
// - Creates a session (sid) cookie (httpOnly, Secure, SameSite=Lax)
// - Exposes health endpoints, and minimal admin utilities later

const express = require('express');
const crypto = require('crypto');
const https = require('https');
const fs = require('fs');
const path = require('path');
const sqlite3 = require('sqlite3').verbose();
const { v4: uuidv4 } = require('uuid');

// Config via file (to avoid manual env mgmt): auth.config.json at repo root if exists
// If not found, fallback to built-in defaults (from Wasm.cc as you mentioned)
// Highly recommended to customize in production.
const rootConfigPath = path.join(__dirname, '..', '..', 'auth.config.json');
let CFG = {
  DISCORD_CLIENT_ID: "1431349079459364954",
  DISCORD_CLIENT_SECRET: "7rKcr9JJxN0OhVnSzDfSRV2k5kxcp31M", // Replace in your private repo!
  SITE_BASE_URL: "https://spetals.io",
  DB_PATH: path.join(__dirname, '..', '..', 'data.db'),
  COOKIE_NAME: 'sid',
  COOKIE_TTL_MS: 1000 * 60 * 60 * 8 // 8 hours
};
if (fs.existsSync(rootConfigPath)) {
  try {
    const loaded = JSON.parse(fs.readFileSync(rootConfigPath, 'utf8'));
    CFG = { ...CFG, ...loaded };
    console.log('Loaded auth.config.json');
  } catch(e) {
    console.warn('Could not parse auth.config.json, using defaults');
  }
}

const DISCORD = {
  authorize: 'https://discord.com/api/oauth2/authorize',
  token: 'https://discord.com/api/oauth2/token',
  me: 'https://discord.com/api/users/@me'
};

function makeToken() { return crypto.randomBytes(32).toString('hex'); }
function nowSec() { return Math.floor(Date.now()/1000); }

function parseCookies(header) {
  const out = {};
  if (!header) return out;
  header.split(';').forEach((c) => {
    const idx = c.indexOf('=');
    if (idx === -1) return;
    const k = c.slice(0, idx).trim();
    const v = c.slice(idx + 1).trim();
    out[k] = decodeURIComponent(v);
  });
  return out;
}

function setCookie(res, name, value, opts) {
  const opt = opts || {};
  let c = `${name}=${encodeURIComponent(value)}; Path=/; HttpOnly; SameSite=Lax`;
  if (opt.maxAge) c += `; Max-Age=${Math.floor(opt.maxAge/1000)}`;
  // If behind TLS/https, set Secure. Your site uses https via certbot, so we set Secure by default.
  c += '; Secure';
  res.setHeader('Set-Cookie', c);
}

// SQLite init & schema
const DB_PATH = CFG.DB_PATH;
if (!fs.existsSync(DB_PATH)) fs.writeFileSync(DB_PATH, '');
const db = new sqlite3.Database(DB_PATH);

db.serialize(() => {
  db.run('PRAGMA journal_mode=WAL');
  db.run(`CREATE TABLE IF NOT EXISTS accounts (
    id TEXT PRIMARY KEY,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    banned INTEGER NOT NULL DEFAULT 0,
    ban_reason TEXT
  )`);
  db.run(`CREATE TABLE IF NOT EXISTS discord_links (
    account_id TEXT NOT NULL,
    discord_user_id TEXT NOT NULL UNIQUE,
    created_at INTEGER NOT NULL,
    FOREIGN KEY(account_id) REFERENCES accounts(id)
  )`);
  db.run(`CREATE TABLE IF NOT EXISTS sessions (
    id TEXT PRIMARY KEY,
    account_id TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    expires_at INTEGER NOT NULL,
    revoked INTEGER NOT NULL DEFAULT 0,
    user_agent_hash TEXT,
    last_ip_hash TEXT,
    FOREIGN KEY(account_id) REFERENCES accounts(id)
  )`);
  db.run('CREATE INDEX IF NOT EXISTS idx_sessions_account ON sessions(account_id)');
  db.run(`CREATE TABLE IF NOT EXISTS account_mobs (
    account_id TEXT NOT NULL,
    mob_id INTEGER NOT NULL,
    created_at INTEGER NOT NULL,
    PRIMARY KEY(account_id, mob_id),
    FOREIGN KEY(account_id) REFERENCES accounts(id)
  )`);
});

async function exchangeCodeForToken(code, redirect_uri) {
  const params = new URLSearchParams();
  params.append('client_id', CFG.DISCORD_CLIENT_ID);
  params.append('client_secret', CFG.DISCORD_CLIENT_SECRET);
  params.append('grant_type', 'authorization_code');
  params.append('code', code);
  params.append('redirect_uri', redirect_uri);
  return new Promise((resolve, reject) => {
    const req = https.request(DISCORD.token, { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }}, (res) => {
      let data = '';
      res.on('data', (c) => data += c);
      res.on('end', () => { try { resolve(JSON.parse(data)); } catch(e){ reject(e); } });
    });
    req.on('error', reject);
    req.write(params.toString());
    req.end();
  });
}

async function fetchUser(access_token) {
  return new Promise((resolve, reject) => {
    const req = https.request(DISCORD.me, { headers: { 'Authorization': 'Bearer ' + access_token }}, (res) => {
      let data = '';
      res.on('data', (c) => data += c);
      res.on('end', () => { try { resolve(JSON.parse(data)); } catch(e){ reject(e); } });
    });
    req.on('error', reject);
    req.end();
  });
}

const app = express();
app.use(express.json());


app.get('/auth/discord/login', (req, res) => {
  const redirect_uri = `${CFG.SITE_BASE_URL}/auth/discord/callback`;
  const url = new URL(DISCORD.authorize);
  url.searchParams.set('client_id', CFG.DISCORD_CLIENT_ID);
  url.searchParams.set('redirect_uri', redirect_uri);
  url.searchParams.set('response_type', 'code');
  url.searchParams.set('scope', 'identify');
  res.redirect(url.toString());
});

app.get('/auth/discord/callback', async (req, res) => {
  try {
    const code = req.query.code;
    if (!code) return res.status(400).send('Missing code');
    const redirect_uri = `${CFG.SITE_BASE_URL}/auth/discord/callback`;
    const tokenResp = await exchangeCodeForToken(code, redirect_uri);
    if (!tokenResp.access_token) return res.status(400).send('Auth failed');
    const user = await fetchUser(tokenResp.access_token);
    if (!user || !user.id) return res.status(400).send('User fetch failed');

    const now = nowSec();
    // Find or create account linked to this discord user id
    const discord_id = String(user.id);
    let account_id = await new Promise((resolve, reject) => {
      db.get('SELECT account_id FROM discord_links WHERE discord_user_id=?', [discord_id], (err, row) => {
        if (err) return reject(err);
        resolve(row ? row.account_id : null);
      });
    });
    if (!account_id) {
      account_id = uuidv4();
      await new Promise((resolve, reject) => {
        db.run('INSERT INTO accounts (id, created_at, updated_at) VALUES (?, ?, ?)', [account_id, now, now], (err) => err ? reject(err) : resolve());
      });
      await new Promise((resolve, reject) => {
        db.run('INSERT INTO discord_links (account_id, discord_user_id, created_at) VALUES (?, ?, ?)', [account_id, discord_id, now], (err) => err ? reject(err) : resolve());
      });
    } else {
      await new Promise((resolve, reject) => {
        db.run('UPDATE accounts SET updated_at=? WHERE id=?', [now, account_id], (err) => err ? reject(err) : resolve());
      });
    }

    // Create session
    const sid = makeToken();
    const expiresAt = now + Math.floor(CFG.COOKIE_TTL_MS/1000);
    await new Promise((resolve, reject) => {
      db.run('INSERT INTO sessions (id, account_id, created_at, expires_at, revoked) VALUES (?, ?, ?, ?, 0)', [sid, account_id, now, expiresAt], (err) => err ? reject(err) : resolve());
    });

    setCookie(res, CFG.COOKIE_NAME, sid, { maxAge: CFG.COOKIE_TTL_MS });
    res.redirect('/');
  } catch (e) {
    console.error('OAuth Error', e);
    res.status(500).send('OAuth Error');
  }
});

// Health check
app.get('/auth/health', (req, res) => res.status(200).send('OK'));

// Minimal debug/me endpoint (optional)
app.get('/auth/me', (req, res) => {
  const cookies = parseCookies(req.headers.cookie || '');
  const sid = cookies[CFG.COOKIE_NAME];
  if (!sid) return res.status(401).send('Unauthorized');
  db.get('SELECT sessions.account_id, accounts.banned FROM sessions JOIN accounts ON sessions.account_id = accounts.id WHERE sessions.id=? AND sessions.revoked=0 AND sessions.expires_at > ? LIMIT 1', [sid, nowSec()], (err, row) => {
    if (err) return res.status(500).send('DB error');
    if (!row) return res.status(401).send('Unauthorized');
    if (row.banned) return res.status(403).send('Banned');
    res.json({ account_id: row.account_id });
  });
});

// Centralized account bootstrap (fetch all account-owned data)
app.get('/api/account/bootstrap', (req, res) => {
  const cookies = parseCookies(req.headers.cookie || '');
  const sid = cookies[CFG.COOKIE_NAME];
  if (!sid) return res.status(401).send('Unauthorized');
  const now = nowSec();
  db.get('SELECT sessions.account_id, accounts.banned FROM sessions JOIN accounts ON sessions.account_id = accounts.id WHERE sessions.id=? AND sessions.revoked=0 AND sessions.expires_at > ? LIMIT 1', [sid, now], (err, sess) => {
    if (err) return res.status(500).send('DB error');
    if (!sess) return res.status(401).send('Unauthorized');
    if (sess.banned) return res.status(403).send('Banned');
    db.all('SELECT mob_id FROM account_mobs WHERE account_id=?', [sess.account_id], (e2, rows) => {
      if (e2) return res.status(500).send('DB error');
      const mobs = (rows || []).map(r => r.mob_id | 0);
      res.json({ mobs });
    });
  });
});

// Account mob gallery endpoints
app.get('/api/account/mobs', (req, res) => {
  const cookies = parseCookies(req.headers.cookie || '');
  const sid = cookies[CFG.COOKIE_NAME];
  if (!sid) return res.status(401).send('Unauthorized');
  const now = nowSec();
  db.get('SELECT sessions.account_id, accounts.banned FROM sessions JOIN accounts ON sessions.account_id = accounts.id WHERE sessions.id=? AND sessions.revoked=0 AND sessions.expires_at > ? LIMIT 1', [sid, now], (err, sess) => {
    if (err) return res.status(500).send('DB error');
    if (!sess) return res.status(401).send('Unauthorized');
    if (sess.banned) return res.status(403).send('Banned');
    db.all('SELECT mob_id FROM account_mobs WHERE account_id=?', [sess.account_id], (e2, rows) => {
      if (e2) return res.status(500).send('DB error');
      res.json((rows || []).map(r => r.mob_id | 0));
    });
  });
});

app.post('/api/account/mobs', (req, res) => {
  const cookies = parseCookies(req.headers.cookie || '');
  const sid = cookies[CFG.COOKIE_NAME];
  if (!sid) return res.status(401).send('Unauthorized');
  const now = nowSec();
  db.get('SELECT sessions.account_id, accounts.banned FROM sessions JOIN accounts ON sessions.account_id = accounts.id WHERE sessions.id=? AND sessions.revoked=0 AND sessions.expires_at > ? LIMIT 1', [sid, now], (err, sess) => {
    if (err) return res.status(500).send('DB error');
    if (!sess) return res.status(401).send('Unauthorized');
    if (sess.banned) return res.status(403).send('Banned');
    let mobs = req.body && (req.body.mobs || req.body.add || []);
    if (!Array.isArray(mobs)) mobs = [mobs];
    mobs = mobs.map(x => parseInt(x, 10)).filter(x => Number.isFinite(x) && x >= 0 && x < 256);
    if (mobs.length === 0) return res.status(200).send('OK');
    const stmt = db.prepare('INSERT OR IGNORE INTO account_mobs (account_id, mob_id, created_at) VALUES (?, ?, ?)');
    for (const m of mobs) stmt.run(sess.account_id, m, now);
    stmt.finalize(err2 => {
      if (err2) return res.status(500).send('DB error');
      res.status(200).send('OK');
    });
  });
});

const PORT = process.env.AUTH_PORT ? parseInt(process.env.AUTH_PORT, 10) : 3000;
app.listen(PORT, () => {
  console.log('Auth service listening on port', PORT);
});
