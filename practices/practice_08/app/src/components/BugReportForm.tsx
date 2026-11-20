import { useRef, useState } from 'react'
import Alert from '@mui/material/Alert'
import Box from '@mui/material/Box'
import Button from '@mui/material/Button'
import CircularProgress from '@mui/material/CircularProgress'
import Paper from '@mui/material/Paper'
import Stack from '@mui/material/Stack'
import TextField from '@mui/material/TextField'
import Typography from '@mui/material/Typography'
import { api, type Issue, type NewIssue } from '../api'

const TITLE_MIN = 8
const BODY_MIN = 20

interface Props {
  onCreated: (issue: Issue) => void
}

export function BugReportForm({ onCreated }: Props) {
  const [title, setTitle] = useState('')
  const [body, setBody] = useState('')
  const [errors, setErrors] = useState<Partial<Record<'title' | 'body', string>>>({})
  const [submitError, setSubmitError] = useState('')
  const [pending, setPending] = useState(false)
  const inFlight = useRef(false)

  function validate(next: NewIssue) {
    const nextErrors: typeof errors = {}
    if (next.title.trim().length < TITLE_MIN) {
      nextErrors.title = `Заголовок короче ${TITLE_MIN} символов — опишите симптом`
    }
    if (next.body.trim().length < BODY_MIN) {
      nextErrors.body = `Добавьте шаги воспроизведения (минимум ${BODY_MIN} символов)`
    }
    return nextErrors
  }

  async function handleSubmit(event: React.FormEvent) {
    event.preventDefault()
    setSubmitError('')
    const payload = { title: title.trim(), body: body.trim() }
    const nextErrors = validate(payload)
    setErrors(nextErrors)
    if (Object.keys(nextErrors).length > 0) return
    if (inFlight.current) return
    inFlight.current = true
    setPending(true)
    try {
      const created = await api.create(payload)
      onCreated(created)
      setTitle('')
      setBody('')
      setErrors({})
    } catch (err) {
      setSubmitError(err instanceof Error ? err.message : 'Не удалось отправить репорт')
    } finally {
      inFlight.current = false
      setPending(false)
    }
  }

  return (
    <Paper component="form" onSubmit={handleSubmit} noValidate sx={{ p: 3 }} aria-label="Форма баг-репорта">
      <Typography variant="h6" component="h2" gutterBottom>
        Новый баг-репорт
      </Typography>
      <Typography variant="body2" color="text.secondary" sx={{ mb: 2 }}>
        Репорт уйдёт в трекер с лейблом bug, дальше его подхватит автоматический фикс.
      </Typography>
      <Stack spacing={2}>
        <TextField
          id="report-title"
          label="Заголовок"
          placeholder="Что сломалось"
          value={title}
          onChange={(e) => setTitle(e.target.value)}
          error={Boolean(errors.title)}
          helperText={errors.title ?? ' '}
          inputProps={{ 'aria-describedby': 'report-title-help' }}
          disabled={pending}
        />
        <TextField
          id="report-body"
          label="Шаги воспроизведения"
          placeholder="1. Открыть отчёт&#10;2. Оставить список пустым"
          value={body}
          onChange={(e) => setBody(e.target.value)}
          error={Boolean(errors.body)}
          helperText={errors.body ?? ' '}
          multiline
          minRows={4}
          disabled={pending}
        />
        {submitError && (
          <Alert severity="error" role="alert" action={
            <Button color="inherit" size="small" onClick={() => setSubmitError('')}>
              Понятно
            </Button>
          }>
            {submitError}
          </Alert>
        )}
        <Box sx={{ display: 'flex', alignItems: 'center', gap: 2 }}>
          <Button type="submit" variant="contained" disabled={pending}>
            {pending ? 'Отправляем…' : 'Отправить репорт'}
          </Button>
          {pending && <CircularProgress size={20} aria-label="Отправка" />}
          <Typography variant="caption" color="text.secondary">
            Ctrl+Enter тоже отправляет
          </Typography>
        </Box>
      </Stack>
    </Paper>
  )
}
