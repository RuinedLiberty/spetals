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
  // Optional users table for vanity names (Discord username cache)
  db.run(`CREATE TABLE IF NOT EXISTS users (
    discord_id TEXT PRIMARY KEY,
    username TEXT,
    created_at INTEGER
  )`);
  // Migration: ensure accounts.account_xp exists (used for account leaderboard)
  try {
    db.all('PRAGMA table_info(accounts)', [], (err, rows) => {
      if (err) return;
      const has = Array.isArray(rows) && rows.some(r => r && r.name === 'account_xp');
      if (!has) {
        try { db.run('ALTER TABLE accounts ADD COLUMN account_xp INTEGER NOT NULL DEFAULT 0'); } catch(e) {}
      }
    });
  } catch(e) {}
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

// Compute account-level XP curve (mirrors C++), multiplier configurable via env ACCOUNT_XP_MULT (default 100)
const MAX_LEVEL = 99;
function scoreToPassLevel(level) {
  return Math.floor(Math.pow(1.06, level - 1) * level) + 3;
}
function accountXpToLevel(totalXp) {
  const M = parseInt(process.env.ACCOUNT_XP_MULT || '100', 10) || 100;
  let level = 1;
  let remaining = Math.max(0, Number(totalXp|0));
  while (level < MAX_LEVEL) {
    const need = scoreToPassLevel(level) * M;
    if (remaining < need) break;
    remaining -= need;
    level++;
  }
  return { level, xp: remaining, xpNeeded: (level >= MAX_LEVEL) ? 0xffffffff : scoreToPassLevel(level) * M };
}

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

// Minimal debug/me endpoint (extended with username/xp/level)
app.get('/auth/me', async (req, res) => {
  try {
    const cookies = parseCookies(req.headers.cookie || '');
    const sid = cookies[CFG.COOKIE_NAME];
    if (!sid) { return res.status(401).send('Unauthorized'); }
    const now = nowSec();
    const sess = await new Promise((resolve)=>{
      db.get('SELECT account_id FROM sessions WHERE id=? AND revoked=0 AND expires_at > ? LIMIT 1', [sid, now], (err, row) => resolve(err ? null : row));
    });
    if (!sess || !sess.account_id) { return res.status(401).send('Unauthorized'); }
    const acc = sess.account_id;
    const banRow = await new Promise((resolve)=>{
      db.get('SELECT banned FROM accounts WHERE id=? LIMIT 1', [acc], (err,row)=>resolve(err?null:row));
    });
    if (banRow && Number(banRow.banned)) { return res.status(403).send('Banned'); }
    const link = await new Promise((resolve)=>{
      db.get('SELECT discord_user_id AS did FROM discord_links WHERE account_id=? LIMIT 1', [acc], (err,row)=>resolve(err?null:row));
    });
    const unameRow = link ? await new Promise((resolve)=>{
      db.get('SELECT username FROM users WHERE discord_id=? LIMIT 1', [link.did], (err,row)=>resolve(err?null:row));
    }) : null;
    const xpRow = await new Promise((resolve)=>{
      db.get('SELECT account_xp AS xp FROM accounts WHERE id=? LIMIT 1', [acc], (err,row)=>resolve(err?null:row));
    });
    const xp = (xpRow && xpRow.xp) ? (xpRow.xp|0) : 0;
        const lvlInfo = accountXpToLevel(xp|0);
    const payload = {
      account_id: acc,
      discord_id: link && link.did ? String(link.did) : '',
      username: (unameRow && unameRow.username) ? String(unameRow.username) : '',
      account_xp: xp|0,
      account_level: lvlInfo.level|0,
      xpNeeded: lvlInfo.xpNeeded|0
    };

    res.json(payload);
  } catch(e) {
    
    res.status(500).send('DB error');
  }
});

// Public leaderboard of top accounts by account_xp (offline included)
app.get('/auth/leaderboard', (req, res) => {
  try {
    const limitRaw = req.query.limit;
    let limit = 50;
    if (typeof limitRaw !== 'undefined') {
      const n = parseInt(String(limitRaw), 10);
      if (!Number.isNaN(n) && n > 0) limit = n;
    }
    limit = Math.min(limit, 200);

    const sql = `
      SELECT a.account_xp AS xp, dl.discord_user_id AS did, u.username AS uname
      FROM accounts a
      LEFT JOIN discord_links dl ON dl.account_id = a.id
      LEFT JOIN users u ON u.discord_id = dl.discord_user_id
      WHERE a.account_xp > 0
      ORDER BY a.account_xp DESC
      LIMIT ?
    `;

    db.all(sql, [limit], (err, rows) => {
      if (err) { return res.status(500).send('DB error'); }

      const out = (rows || []).map(r => {
        const info = accountXpToLevel(r.xp || 0);
        const name = (r.uname && String(r.uname)) || (r.did && String(r.did)) || 'Unnamed';
        const row = { name, level: info.level, xp: info.xp, xpNeeded: info.xpNeeded };
        return row;
      });

      res.json(out);
    });
  } catch (e) {

    res.status(500).send('Error');
  }
});

// Count of accounts with non-zero XP
app.get('/auth/leaderboard/count', (req, res) => {
    try {
    const sql = 'SELECT COUNT(*) AS total FROM accounts WHERE account_xp > 0';
    db.get(sql, [], (err, row) => {
      if (err) { return res.status(500).send('DB error'); }
      const total = row && row.total ? (row.total|0) : 0;
      res.json({ total });
    });

  } catch (e) {
    res.status(500).send('Error');
  }
});

// Compatibility alias for clients expecting /api/leaderboard
app.get('/api/leaderboard', (req, res) => {
  try {
    const limitRaw = req.query.limit;
    let limit = 50;
    if (typeof limitRaw !== 'undefined') {
      const n = parseInt(String(limitRaw), 10);
      if (!Number.isNaN(n) && n > 0) limit = n;
    }
    limit = Math.min(limit, 200);

    const sql = `
      SELECT a.account_xp AS xp, dl.discord_user_id AS did, u.username AS uname
      FROM accounts a
      LEFT JOIN discord_links dl ON dl.account_id = a.id
      LEFT JOIN users u ON u.discord_id = dl.discord_user_id
      WHERE a.account_xp > 0
      ORDER BY a.account_xp DESC
      LIMIT ?
    `;

    db.all(sql, [limit], (err, rows) => {
      if (err) { return res.status(500).send('DB error'); }

      const out = (rows || []).map(r => {
        const info = accountXpToLevel(r.xp || 0);
        const name = (r.uname && String(r.uname)) || (r.did && String(r.did)) || 'Unnamed';
        return { name, level: info.level, xp: info.xp, xpNeeded: info.xpNeeded };
      });

      res.json(out);
    });
  } catch (e) {

    res.status(500).send('Error');
  }
});

// Count alias
app.get('/api/leaderboard/count', (req, res) => {
    try {
    const sql = 'SELECT COUNT(*) AS total FROM accounts WHERE account_xp > 0';
    db.get(sql, [], (err, row) => {
      if (err) { return res.status(500).send('DB error'); }
      const total = row && row.total ? (row.total|0) : 0;
      res.json({ total });
    });

  } catch (e) {
    res.status(500).send('Error');
  }
});

const PORT = process.env.AUTH_PORT ? parseInt(process.env.AUTH_PORT, 10) : 3000;
app.listen(PORT, () => {
  console.log('Auth service listening on port', PORT);
});
