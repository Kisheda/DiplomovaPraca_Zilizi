import { useState } from "react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Eye, EyeOff, Home } from "lucide-react";
import mqtt from "mqtt";

interface LoginPageProps {
  onLogin: () => void;
}

const MQTT_BROKER_URL = import.meta.env.VITE_MQTT_BROKER_URL ?? "wss://b583e186639e4bc2aa7bc95605777610.s1.eu.hivemq.cloud:8884/mqtt";

export default function LoginPage({ onLogin }: LoginPageProps) {
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [showPassword, setShowPassword] = useState(false);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    setError("");
    setLoading(true);

    const client = mqtt.connect(MQTT_BROKER_URL, {
      clientId: `login-${Math.random().toString(16).slice(2)}`,
      username,
      password,
      keepalive: 10,
      clean: true,
      reconnectPeriod: 0,       // don't retry
      connectTimeout: 8000,
    });

    const timeout = setTimeout(() => {
      client.end(true);
      setError("Connection timed out. Please try again.");
      setLoading(false);
    }, 9000);

    client.on("connect", () => {
      clearTimeout(timeout);
      client.end(true);
      onLogin();
    });

    client.on("error", (err) => {
      clearTimeout(timeout);
      client.end(true);
      const msg = err.message.toLowerCase();
      if (msg.includes("bad user") || msg.includes("not authorized") || msg.includes("unauthorized")) {
        setError("Invalid username or password.");
      } else {
        setError("Connection failed. Check your network.");
      }
      setLoading(false);
    });
  };

  return (
    <div className="min-h-screen bg-slate-50 flex items-center justify-center p-6">
      <div className="w-full max-w-sm space-y-6">
        <div className="flex flex-col items-center gap-3">
          <div className="flex h-14 w-14 items-center justify-center rounded-2xl bg-white shadow-sm">
            <Home className="h-7 w-7 text-slate-700" />
          </div>
          <div className="text-center">
            <h1 className="text-2xl font-bold tracking-tight">Smart Home</h1>
            <p className="text-sm text-slate-500">Sign in to continue</p>
          </div>
        </div>

        <Card className="rounded-3xl shadow-sm">
          <CardHeader className="pb-2">
            <CardTitle className="text-base font-semibold">Login</CardTitle>
          </CardHeader>
          <CardContent>
            <form onSubmit={handleSubmit} className="space-y-4">
              <div className="space-y-1.5">
                <label className="text-xs font-medium text-slate-500">Username</label>
                <input
                  type="text"
                  value={username}
                  onChange={(e) => setUsername(e.target.value)}
                  autoComplete="username"
                  required
                  className="w-full rounded-xl border bg-slate-50 px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-slate-300"
                  placeholder="Enter username"
                />
              </div>

              <div className="space-y-1.5">
                <label className="text-xs font-medium text-slate-500">Password</label>
                <div className="relative">
                  <input
                    type={showPassword ? "text" : "password"}
                    value={password}
                    onChange={(e) => setPassword(e.target.value)}
                    autoComplete="current-password"
                    required
                    className="w-full rounded-xl border bg-slate-50 px-3 py-2 pr-9 text-sm outline-none focus:ring-2 focus:ring-slate-300"
                    placeholder="Enter password"
                  />
                  <button
                    type="button"
                    onClick={() => setShowPassword((v) => !v)}
                    className="absolute right-2.5 top-1/2 -translate-y-1/2 text-slate-400 hover:text-slate-600"
                    tabIndex={-1}
                  >
                    {showPassword ? <EyeOff className="h-4 w-4" /> : <Eye className="h-4 w-4" />}
                  </button>
                </div>
              </div>

              {error && (
                <p className="text-xs text-red-500 font-medium">{error}</p>
              )}

              <Button type="submit" className="w-full" disabled={loading}>
                {loading ? "Signing in..." : "Sign in"}
              </Button>
            </form>
          </CardContent>
        </Card>
      </div>
    </div>
  );
}
