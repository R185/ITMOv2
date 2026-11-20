import { afterAll, afterEach, beforeAll } from 'vitest'
import '@testing-library/jest-dom/vitest'
import { server } from '../mocks/server'

beforeAll(() => server.listen({ onUnhandledRequest: 'error' }))
afterEach(() => server.resetHandlers())
afterAll(() => server.close())

// jsdom не умеет matchMedia — MUI спрашивает его для адаптивных стилей
Object.defineProperty(window, 'matchMedia', {
  writable: true,
  value: (query: string) => ({
    matches: false,
    media: query,
    onchange: null,
    addListener: () => {},
    removeListener: () => {},
    addEventListener: () => {},
    removeEventListener: () => {},
    dispatchEvent: () => false
  })
})
