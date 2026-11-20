import { describe, expect, it } from 'vitest'
import { fireEvent, render, screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { http, HttpResponse, delay } from 'msw'
import App from '../App'
import { server } from '../mocks/server'

function renderApp() {
  return render(<App />)
}

const fillForm = async (user: ReturnType<typeof userEvent.setup>) => {
  await user.type(screen.getByLabelText('Заголовок'), 'Отчёт падает на пустом списке')
  await user.type(
    screen.getByLabelText('Шаги воспроизведения'),
    'Открыть отчёт, оставить список пустым, нажать «Сформировать»'
  )
}

describe('Bug Triage Console', () => {
  it('показывает репорты из API со статусами и ссылкой на PR', async () => {
    renderApp()
    expect(await screen.findByText('summary([]) падает на пустом списке')).toBeVisible()
    expect(screen.getByLabelText('Статус: Исправлен')).toBeVisible()
    expect(screen.getByLabelText('Статус: В работе')).toBeVisible()
    expect(screen.getByRole('link', { name: /PR #24/i })).toBeVisible()
  })

  it('показывает пустое состояние, когда репортов нет', async () => {
    server.use(http.get(/\/api\/issues$/, () => HttpResponse.json([])))
    renderApp()
    expect(await screen.findByText('Пока пусто')).toBeVisible()
  })

  it('показывает ошибку загрузки и повторяет запрос по кнопке', async () => {
    const user = userEvent.setup()
    let calls = 0
    server.use(
      http.get(/\/api\/issues$/, () => {
        calls += 1
        if (calls === 1) return HttpResponse.json({ message: 'сервис недоступен' }, { status: 502 })
        return HttpResponse.json([])
      })
    )
    renderApp()
    const alert = await screen.findByRole('alert')
    expect(alert).toHaveTextContent(/сервис недоступен/i)
    await user.click(screen.getByRole('button', { name: 'Повторить' }))
    expect(await screen.findByText('Пока пусто')).toBeVisible()
    expect(calls).toBe(2)
  })

  it('валидирует обязательные поля и не отправляет запрос', async () => {
    const user = userEvent.setup()
    let posts = 0
    server.use(
      http.post(/\/api\/issues$/, () => {
        posts += 1
        return HttpResponse.json({})
      })
    )
    renderApp()
    await screen.findByText('summary([]) падает на пустом списке')
    await user.click(screen.getByRole('button', { name: 'Отправить репорт' }))
    expect(await screen.findByText(/Заголовок короче 8 символов/i)).toBeVisible()
    expect(screen.getByText(/Добавьте шаги воспроизведения/i)).toBeVisible()
    expect(posts).toBe(0)
  })

  it('создаёт репорт и показывает его в списке', async () => {
    const user = userEvent.setup()
    renderApp()
    await screen.findByText('summary([]) падает на пустом списке')
    await fillForm(user)
    await user.click(screen.getByRole('button', { name: 'Отправить репорт' }))
    expect(await screen.findByText('Отчёт падает на пустом списке')).toBeVisible()
    expect(await screen.findByText(/Репорт #99 создан/i)).toBeVisible()
    expect(screen.getByLabelText('Заголовок')).toHaveValue('')
  })

  it('не отправляет второй запрос при повторном клике во время ожидания', async () => {
    const user = userEvent.setup()
    let posts = 0
    server.use(
      http.post(/\/api\/issues$/, async ({ request }) => {
        posts += 1
        const body = (await request.json()) as { title: string; body: string }
        await delay(300)
        return HttpResponse.json({ number: 77, ...body, state: 'open', labels: ['bug'], createdAt: '' }, { status: 201 })
      })
    )
    renderApp()
    await screen.findByText('summary([]) падает на пустом списке')
    await fillForm(user)
    const submit = screen.getByRole('button', { name: 'Отправить репорт' })
    await user.click(submit)
    // пока идёт запрос кнопка и поля disabled; повторную отправку ловит guard в компоненте
    const form = screen.getByRole('form', { name: 'Форма баг-репорта' })
    fireEvent.submit(form)
    fireEvent.submit(form)
    await waitFor(() => expect(posts).toBe(1))
    expect(await screen.findByText(/Репорт #77 создан/i)).toBeVisible()
    expect(posts).toBe(1)
  })

  it('показывает понятную ошибку сервера и даёт повторить отправку', async () => {
    const user = userEvent.setup()
    let posts = 0
    server.use(
      http.post(/\/api\/issues$/, () => {
        posts += 1
        if (posts === 1) return HttpResponse.json({ message: 'трекер недоступен' }, { status: 500 })
        return HttpResponse.json({ number: 55, title: 't', body: 'b', state: 'open', labels: ['bug'], createdAt: '' }, { status: 201 })
      })
    )
    renderApp()
    await screen.findByText('summary([]) падает на пустом списке')
    await fillForm(user)
    await user.click(screen.getByRole('button', { name: 'Отправить репорт' }))
    expect(await screen.findByRole('alert')).toHaveTextContent(/трекер недоступен/i)
    expect(screen.getByLabelText('Заголовок')).toHaveValue('Отчёт падает на пустом списке')
    await user.click(screen.getByRole('button', { name: 'Отправить репорт' }))
    await waitFor(() => expect(posts).toBe(2))
    expect(await screen.findByText(/Репорт #55 создан/i)).toBeVisible()
  })

  it('держит подписи полей и доступные с клавиатуры элементы', async () => {
    const user = userEvent.setup()
    renderApp()
    await screen.findByText('summary([]) падает на пустом списке')
    expect(screen.getByLabelText('Заголовок')).toBeInTheDocument()
    expect(screen.getByLabelText('Шаги воспроизведения')).toBeInTheDocument()
    await user.tab()
    await waitFor(() => expect(document.activeElement).not.toBe(document.body))
  })
})
