import { useCallback, useEffect, useState } from 'react'
import AppBar from '@mui/material/AppBar'
import Box from '@mui/material/Box'
import Container from '@mui/material/Container'
import Grid from '@mui/material/Grid2'
import Snackbar from '@mui/material/Snackbar'
import Toolbar from '@mui/material/Toolbar'
import Typography from '@mui/material/Typography'
import { api, type Issue } from './api'
import { BugReportForm } from './components/BugReportForm'
import { IssueList } from './components/IssueList'

type Phase = 'loading' | 'error' | 'ready'

export default function App() {
  const [phase, setPhase] = useState<Phase>('loading')
  const [issues, setIssues] = useState<Issue[]>([])
  const [error, setError] = useState('')
  const [toast, setToast] = useState('')

  const load = useCallback(async () => {
    setPhase('loading')
    setError('')
    try {
      const data = await api.list()
      setIssues(data)
      setPhase('ready')
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Ошибка сети')
      setPhase('error')
    }
  }, [])

  useEffect(() => {
    void load()
  }, [load])

  function handleCreated(issue: Issue) {
    setIssues((prev) => [issue, ...prev])
    setPhase('ready')
    setToast(`Репорт #${issue.number} создан`)
  }

  return (
    <Box sx={{ minHeight: '100vh', bgcolor: 'background.default' }}>
      <AppBar position="static" color="primary" elevation={0}>
        <Toolbar>
          <Typography variant="h6" component="h1" sx={{ flexGrow: 1 }}>
            Bug Triage Console
          </Typography>
          <Typography variant="body2" sx={{ opacity: 0.85 }}>
            issue → патч → ревью → мерж
          </Typography>
        </Toolbar>
      </AppBar>

      <Container maxWidth="lg" sx={{ py: 4 }}>
        <Grid container spacing={3}>
          <Grid size={{ xs: 12, md: 5 }}>
            <BugReportForm onCreated={handleCreated} />
          </Grid>
          <Grid size={{ xs: 12, md: 7 }}>
            <Typography variant="h5" component="h2" gutterBottom>
              Репорты
            </Typography>
            <IssueList phase={phase} issues={issues} error={error} onRetry={() => void load()} />
          </Grid>
        </Grid>
      </Container>

      <Snackbar
        open={Boolean(toast)}
        autoHideDuration={4000}
        onClose={() => setToast('')}
        message={toast}
      />
    </Box>
  )
}
