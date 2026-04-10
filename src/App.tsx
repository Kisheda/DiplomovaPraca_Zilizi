import { useState } from 'react'
import SmartHomeDashboard from './SmartHomeDashboard'
import LogsPage from './LogsPage'
import LoginPage from './LoginPage'

function App() {
  const [page, setPage] = useState<"login" | "dashboard" | "logs">("login");

  if (page === "login") {
    return <LoginPage onLogin={() => setPage("dashboard")} />;
  }

  if (page === "logs") {
    return <LogsPage onBack={() => setPage("dashboard")} />;
  }

  return <SmartHomeDashboard onLogsClick={() => setPage("logs")} onLogout={() => setPage("login")} />;
}

export default App
