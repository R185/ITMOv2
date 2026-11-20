#!/usr/bin/env node
/**
 * Bug Triage Console — тонкий сервер: отдаёт собранный фронтенд и проксирует GitHub API.
 * Токен читается из GITHUB_TOKEN или из ~/.config/gh/hosts.yml и никогда не уходит в браузер.
 */
import http from 'node:http'
import fs from 'node:fs'
import path from 'node:path'
import os from 'node:os'
import { fileURLToPath } from 'node:url'

const HERE = path.dirname(fileURLToPath(import.meta.url))
const DIST = path.join(HERE, '..', 'app', 'dist')
const PORT = Number(process.env.PORT || 8787)
const REPO = process.env.REPO || 'R185/itmo-ai-practice-07'

function readToken() {
  if (process.env.GITHUB_TOKEN) return process.env.GITHUB_TOKEN.trim()
  try {
    const yaml = fs.readFileSync(path.join(os.homedir(), '.config', 'gh', 'hosts.yml'), 'utf8')
    const line = yaml.split('\n').find((l) => l.trim().startsWith('oauth_token:'))
    if (line) return line.split(':')[1].trim()
  } catch {}
  return ''
}

const TOKEN = readToken()
if (!TOKEN) {
  console.error('нет GitHub-токена: задайте GITHUB_TOKEN или авторизуйте gh')
  process.exit(1)
}

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.json': 'application/json; charset=utf-8'
}

async function gh(pathname, init = {}) {
  const res = await fetch(`https://api.github.com${pathname}`, {
    ...init,
    headers: {
      Authorization: `Bearer ${TOKEN}`,
      Accept: 'application/vnd.github+json',
      'User-Agent': 'bug-triage-console',
      'Content-Type': 'application/json',
      ...(init.headers || {})
    }
  })
  const text = await res.text()
  if (!res.ok) {
    const err = new Error(`GitHub ${res.status}: ${text.slice(0, 300)}`)
    err.status = res.status
    throw err
  }
  return text ? JSON.parse(text) : null
}

const prCache = { at: 0, value: [] }
async function pulls() {
  if (Date.now() - prCache.at < 30_000) return prCache.value
  const list = await gh(`/repos/${REPO}/pulls?state=all&per_page=100`)
  prCache.at = Date.now()
  prCache.value = list
    .filter((p) => p.state === 'open' || p.merged_at)
    .map((p) => ({ number: p.number, url: p.html_url, head: p.head.ref, merged: Boolean(p.merged_at) }))
  return prCache.value
}

function toIssue(raw, prs) {
  const pr = prs.find((p) => p.head === `fix/issue-${raw.number}`)
  const state = pr?.merged ? 'fixed' : pr ? 'triaged' : 'open'
  return {
    number: raw.number,
    title: raw.title,
    body: raw.body || '',
    state,
    labels: (raw.labels || []).map((l) => (typeof l === 'string' ? l : l.name)),
    createdAt: raw.created_at,
    prNumber: pr?.number,
    prUrl: pr?.url
  }
}

function json(res, status, payload) {
  const body = JSON.stringify(payload)
  res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Content-Length': Buffer.byteLength(body) })
  res.end(body)
}

function serveStatic(req, res, url) {
  let file = path.join(DIST, url.pathname === '/' ? 'index.html' : url.pathname)
  if (!file.startsWith(DIST)) return json(res, 403, { message: 'forbidden' })
  if (!fs.existsSync(file) || fs.statSync(file).isDirectory()) file = path.join(DIST, 'index.html')
  const ext = path.extname(file)
  res.writeHead(200, { 'Content-Type': MIME[ext] || 'application/octet-stream' })
  fs.createReadStream(file).pipe(res)
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, `http://${req.headers.host}`)

  if (url.pathname === '/api/issues' && req.method === 'GET') {
    try {
      const [issues, prs] = await Promise.all([
        gh(`/repos/${REPO}/issues?state=all&per_page=30&sort=created&direction=desc`),
        pulls()
      ])
      const onlyIssues = issues.filter((i) => !i.pull_request)
      return json(res, 200, onlyIssues.map((i) => toIssue(i, prs)))
    } catch (err) {
      return json(res, 502, { message: err.message })
    }
  }

  if (url.pathname === '/api/issues' && req.method === 'POST') {
    let body = ''
    for await (const chunk of req) body += chunk
    try {
      const payload = JSON.parse(body || '{}')
      const title = String(payload.title || '').trim()
      const text = String(payload.body || '').trim()
      if (title.length < 8 || text.length < 20) {
        return json(res, 422, { message: 'Заголовок минимум 8 символов, описание — 20' })
      }
      const created = await gh(`/repos/${REPO}/issues`, {
        method: 'POST',
        body: JSON.stringify({ title, body: text, labels: ['bug'] })
      })
      prCache.at = 0
      return json(res, 201, toIssue(created, []))
    } catch (err) {
      return json(res, 502, { message: err.message })
    }
  }

  const match = url.pathname.match(/^\/api\/issues\/(\d+)$/)
  if (match && req.method === 'GET') {
    try {
      const [issue, prs] = await Promise.all([gh(`/repos/${REPO}/issues/${match[1]}`), pulls()])
      return json(res, 200, toIssue(issue, prs))
    } catch (err) {
      return json(res, 502, { message: err.message })
    }
  }

  if (url.pathname.startsWith('/api/')) return json(res, 404, { message: 'not found' })
  return serveStatic(req, res, url)
})

server.listen(PORT, '127.0.0.1', () => {
  console.log(`bug-triage-console на http://127.0.0.1:${PORT}, репозиторий ${REPO}`)
})
