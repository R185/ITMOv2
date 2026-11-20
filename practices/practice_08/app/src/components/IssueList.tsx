import Alert from '@mui/material/Alert'
import Box from '@mui/material/Box'
import Button from '@mui/material/Button'
import Card from '@mui/material/Card'
import CardContent from '@mui/material/CardContent'
import Chip from '@mui/material/Chip'
import Link from '@mui/material/Link'
import Skeleton from '@mui/material/Skeleton'
import Stack from '@mui/material/Stack'
import Typography from '@mui/material/Typography'
import type { Issue } from '../api'
import { StatusChip } from './StatusChip'

type Phase = 'loading' | 'error' | 'ready'

interface Props {
  phase: Phase
  issues: Issue[]
  error: string
  onRetry: () => void
}

export function IssueList({ phase, issues, error, onRetry }: Props) {
  if (phase === 'loading') {
    return (
      <Stack spacing={2} aria-busy="true" aria-label="Загрузка репортов">
        {[0, 1, 2].map((i) => (
          <Card key={i} variant="outlined">
            <CardContent>
              <Skeleton width="60%" height={28} />
              <Skeleton width="90%" />
              <Skeleton width="40%" />
            </CardContent>
          </Card>
        ))}
      </Stack>
    )
  }

  if (phase === 'error') {
    return (
      <Alert
        severity="error"
        role="alert"
        action={
          <Button color="inherit" size="small" onClick={onRetry}>
            Повторить
          </Button>
        }
      >
        {error || 'Не удалось загрузить список репортов'}
      </Alert>
    )
  }

  if (issues.length === 0) {
    return (
      <Box sx={{ border: '1px dashed', borderColor: 'divider', borderRadius: 3, p: 4, textAlign: 'center' }}>
        <Typography variant="h6" gutterBottom>
          Пока пусто
        </Typography>
        <Typography color="text.secondary">
          Отправьте первый репорт — он появится здесь и уйдёт в автоматический разбор.
        </Typography>
      </Box>
    )
  }

  return (
    <Stack spacing={2} component="ul" sx={{ listStyle: 'none', p: 0, m: 0 }} aria-label="Список репортов">
      {issues.map((issue) => (
        <Box component="li" key={issue.number}>
          <Card variant="outlined">
            <CardContent>
              <Stack direction="row" spacing={1} alignItems="center" sx={{ mb: 1, flexWrap: 'wrap' }}>
                <Typography variant="caption" color="text.secondary">
                  #{issue.number}
                </Typography>
                <StatusChip state={issue.state} />
                {issue.labels.map((label) => (
                  <Chip key={label} size="small" variant="outlined" label={label} />
                ))}
              </Stack>
              <Typography variant="h6" component="h3" gutterBottom>
                {issue.title}
              </Typography>
              <Typography variant="body2" color="text.secondary" sx={{ mb: 1, whiteSpace: 'pre-line' }}>
                {issue.body}
              </Typography>
              {issue.prUrl && (
                <Link href={issue.prUrl} target="_blank" rel="noreferrer" variant="body2">
                  PR #{issue.prNumber} с исправлением
                </Link>
              )}
            </CardContent>
          </Card>
        </Box>
      ))}
    </Stack>
  )
}
