export type IssueState = 'open' | 'triaged' | 'fixed'

export interface Issue {
  number: number
  title: string
  body: string
  state: IssueState
  labels: string[]
  createdAt: string
  prNumber?: number
  prUrl?: string
}

export interface NewIssue {
  title: string
  body: string
}

export class ApiError extends Error {
  constructor(message: string, readonly status?: number) {
    super(message)
    this.name = 'ApiError'
  }
}

const BASE = `${import.meta.env.BASE_URL}api`.replace('//', '/')

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(`${BASE}${path}`, {
    headers: { 'Content-Type': 'application/json' },
    ...init
  })
  if (!res.ok) {
    const text = await res.text().catch(() => '')
    throw new ApiError(text || `Запрос не удался (${res.status})`, res.status)
  }
  return res.json() as Promise<T>
}

export const api = {
  list: () => request<Issue[]>('/issues'),
  get: (number: number) => request<Issue>(`/issues/${number}`),
  create: (payload: NewIssue) =>
    request<Issue>('/issues', { method: 'POST', body: JSON.stringify(payload) })
}
