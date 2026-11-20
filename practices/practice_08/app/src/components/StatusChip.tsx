import Chip from '@mui/material/Chip'
import type { IssueState } from '../api'

const MAP: Record<IssueState, { label: string; color: 'default' | 'info' | 'success' }> = {
  open: { label: 'Открыт', color: 'default' },
  triaged: { label: 'В работе', color: 'info' },
  fixed: { label: 'Исправлен', color: 'success' }
}

export function StatusChip({ state }: { state: IssueState }) {
  const cfg = MAP[state] ?? MAP.open
  return <Chip size="small" color={cfg.color} label={cfg.label} aria-label={`Статус: ${cfg.label}`} />
}
