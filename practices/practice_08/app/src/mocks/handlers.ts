import { http, HttpResponse, delay } from 'msw'
import type { Issue } from '../api'

export const seed: Issue[] = [
  {
    number: 16,
    title: 'summary([]) падает на пустом списке',
    body: 'ValueError: min() arg is an empty sequence',
    state: 'fixed',
    labels: ['bug'],
    createdAt: '2026-11-20T05:00:00Z',
    prNumber: 24,
    prUrl: 'https://github.com/R185/itmo-ai-practice-07/pull/24'
  },
  {
    number: 12,
    title: 'percent_change падает при old = 0',
    body: 'ZeroDivisionError при базовом значении 0',
    state: 'triaged',
    labels: ['bug'],
    createdAt: '2026-11-19T10:00:00Z'
  }
]

export const handlers = [
  http.get(/\/api\/issues$/, async () => {
    await delay(10)
    return HttpResponse.json(seed)
  }),

  http.post(/\/api\/issues$/, async ({ request }) => {
    const payload = (await request.json()) as { title: string; body: string }
    await delay(10)
    return HttpResponse.json(
      {
        number: 99,
        title: payload.title,
        body: payload.body,
        state: 'open',
        labels: ['bug'],
        createdAt: new Date().toISOString()
      } satisfies Issue,
      { status: 201 }
    )
  })
]
