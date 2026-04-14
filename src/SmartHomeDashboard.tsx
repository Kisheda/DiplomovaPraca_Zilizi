import React, { useState, useEffect } from "react";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Input } from "@/components/ui/input";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { AlertTriangle, ShieldAlert, Thermometer, Droplets, Sun, Gauge, Wifi, AlertCircle, ScrollText, LogOut, CreditCard, UserPlus } from "lucide-react";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from "recharts";
import { useSupabaseData } from "@/hooks/useSupabaseData";
import { useMqttData } from "@/hooks/useMqttData";
import { supabase } from "@/lib/supabase";

const formatTimestamp = (value: string) => {
  try {
    return new Date(value).toLocaleTimeString("hu-HU", { hour: "2-digit", minute: "2-digit", hour12: false });
  } catch {
    return value;
  }
};


function MetricCard({ title, value, unit, icon: Icon }: { title: string; value: number; unit: string; icon: React.ComponentType<{ className?: string }> }) {
  return (
    <Card className="rounded-2xl shadow-sm">
      <CardContent className="p-5">
        <div className="flex items-start justify-between">
          <div>
            <p className="text-sm text-slate-500">{title}</p>
            <div className="mt-2 flex items-end gap-1">
              <span className="text-3xl font-semibold">{value.toFixed(1)}</span>
              <span className="pb-1 text-sm text-slate-500">{unit}</span>
            </div>
          </div>
          <div className="rounded-2xl bg-slate-100 p-3">
            <Icon className="h-5 w-5" />
          </div>
        </div>
      </CardContent>
    </Card>
  );
}

export default function SmartHomeDashboard({ onLogsClick, onLogout }: { onLogsClick?: () => void; onLogout?: () => void }) {
  const DEFAULT_WINDOWSHADE_OPEN = "07:00";
  const DEFAULT_WINDOWSHADE_CLOSE = "20:00";

  const isValidTime = (value: string) => /^([01]\d|2[0-3]):([0-5]\d)$/.test(value);

  const {
    historyData,
    loading: historyLoading,
    error: historyError,
  } = useSupabaseData();
  const {
    temperature,
    humidity,
    lightIntensity,
    pressure,
    soilMoisture,
    coStatus,
    meteoStatus,
    windowshadeState,
    windowshadePosition,
    windowshadeStatus,
    displayStatus,
    securityStatus,
    sirenState,
    alarmState,
    airQualityData,
    connected: mqttConnected,
    error: mqttError,
    commandStatus,
    publishCommand,
    modeState,
    relayState,
    publishRelayOn,
    publishRelayOff,
    publishModeAuto,
    publishPumpOn,
    publishEnrollOn,
  } = useMqttData();

  const alarmArmed = alarmState === "ON";
  const [authorizedCardCount, setAuthorizedCardCount] = useState<number | null>(null);
  const [enrollLoading, setEnrollLoading] = useState(false);
  const [windowshadeSettingsId, setWindowshadeSettingsId] = useState<number | null>(null);
  const [windowshadeOpenValue, setWindowshadeOpenValue] = useState<string>(DEFAULT_WINDOWSHADE_OPEN);
  const [windowshadeCloseValue, setWindowshadeCloseValue] = useState<string>(DEFAULT_WINDOWSHADE_CLOSE);
  const [windowshadeSettingsLoading, setWindowshadeSettingsLoading] = useState(true);
  const [windowshadeSettingsSaving, setWindowshadeSettingsSaving] = useState(false);
  const [windowshadeSettingsMessage, setWindowshadeSettingsMessage] = useState<string | null>(null);

  const fetchCardCount = () => {
    supabase
      .from("Authorized_cards")
      .select("id", { count: "exact", head: true })
      .then(({ count }) => setAuthorizedCardCount(count ?? 0));
  };

  useEffect(() => {
    fetchCardCount();
  }, []);

  useEffect(() => {
    const fetchWindowshadeSettings = async () => {
      setWindowshadeSettingsLoading(true);
      setWindowshadeSettingsMessage(null);

      const { data, error } = await supabase
        .from("Settings")
        .select("id,module,settings")
        .eq("module", "windowshade")
        .maybeSingle();

      if (error) {
        setWindowshadeSettingsMessage(`Settings load error: ${error.message}`);
        setWindowshadeSettingsLoading(false);
        return;
      }

      if (data) {
        const settings = (data.settings ?? {}) as { OPEN?: unknown; CLOSE?: unknown };
        const nextOpen = String(settings.OPEN ?? "").trim();
        const nextClose = String(settings.CLOSE ?? "").trim();

        setWindowshadeSettingsId(typeof data.id === "number" ? data.id : Number(data.id));
        setWindowshadeOpenValue(isValidTime(nextOpen) ? nextOpen : DEFAULT_WINDOWSHADE_OPEN);
        setWindowshadeCloseValue(isValidTime(nextClose) ? nextClose : DEFAULT_WINDOWSHADE_CLOSE);
      } else {
        setWindowshadeSettingsId(null);
        setWindowshadeOpenValue(DEFAULT_WINDOWSHADE_OPEN);
        setWindowshadeCloseValue(DEFAULT_WINDOWSHADE_CLOSE);
      }

      setWindowshadeSettingsLoading(false);
    };

    fetchWindowshadeSettings();
  }, []);

  const handleEnrollMode = async () => {
    setEnrollLoading(true);
    publishEnrollOn();
    setTimeout(() => setEnrollLoading(false), 2000);
  };

  const handleSaveWindowshadeSettings = async () => {
    const openTime = windowshadeOpenValue.trim();
    const closeTime = windowshadeCloseValue.trim();

    if (!isValidTime(openTime) || !isValidTime(closeTime)) {
      setWindowshadeSettingsMessage("OPEN es CLOSE formatum: HH:MM (pl. 07:00). ");
      return;
    }

    setWindowshadeSettingsSaving(true);
    setWindowshadeSettingsMessage(null);

    const payload = {
      module: "windowshade",
      settings: {
        OPEN: openTime,
        CLOSE: closeTime,
      },
      updated_at: new Date().toISOString(),
    };

    let updated: { id: number | string } | null = null;
    let updateError: { message: string } | null = null;

    if (windowshadeSettingsId === null) {
      const { data: lastSetting, error: lastSettingError } = await supabase
        .from("Settings")
        .select("id")
        .order("id", { ascending: false })
        .limit(1)
        .maybeSingle();

      if (lastSettingError) {
        setWindowshadeSettingsMessage(`Settings save error: ${lastSettingError.message}`);
        setWindowshadeSettingsSaving(false);
        return;
      }

      const nextId = Number(lastSetting?.id ?? 0) + 1;
      const { data, error } = await supabase
        .from("Settings")
        .insert({ id: nextId, ...payload })
        .select("id")
        .single();

      updated = data;
      updateError = error;
    } else {
      const { data, error } = await supabase
        .from("Settings")
        .update(payload)
        .eq("id", windowshadeSettingsId)
        .select("id")
        .single();

      updated = data;
      updateError = error;
    }

    if (updateError) {
      setWindowshadeSettingsMessage(`Settings save error: ${updateError.message}`);
      setWindowshadeSettingsSaving(false);
      return;
    }

    if (!updated) {
      setWindowshadeSettingsMessage("Settings save error: unknown database response.");
      setWindowshadeSettingsSaving(false);
      return;
    }

    setWindowshadeSettingsId(typeof updated.id === "number" ? updated.id : Number(updated.id));
    setWindowshadeSettingsMessage("Windowshade nyitasi/zaro ido elmentve.");
    setWindowshadeSettingsSaving(false);
  };

  const connectionError = historyError || mqttError;

  const moduleStatus = {
    Meteostanica: meteoStatus === "ONLINE" ? "ONLINE" : "OFFLINE",
    Display: displayStatus === "ONLINE" ? "ONLINE" : "OFFLINE",
    COSensor: coStatus === "1" || coStatus.toLowerCase() === "online" ? "ONLINE" : "OFFLINE",
    Security: securityStatus === "ONLINE" ? "ONLINE" : "OFFLINE",
    Windowshade: windowshadeStatus === "ONLINE" ? "ONLINE" : "OFFLINE",
  } as const;

  if (connectionError) {
    return (
      <div className="min-h-screen bg-slate-50 p-6 flex items-center justify-center">
        <Card className="rounded-2xl shadow-sm border-destructive bg-destructive/5">
          <CardContent className="p-6 flex items-start gap-3">
            <AlertCircle className="h-5 w-5 text-destructive mt-0.5 flex-shrink-0" />
            <div>
              <h3 className="font-semibold text-destructive">Connection Error</h3>
              <p className="text-sm text-destructive/80 mt-1">{connectionError}</p>
            </div>
          </CardContent>
        </Card>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-slate-50 p-6">
      <div className="mx-auto max-w-7xl space-y-6">
        <div className="flex flex-col gap-4 rounded-3xl bg-white p-6 shadow-sm md:flex-row md:items-center md:justify-between">
          <div>
            <h1 className="text-3xl font-bold tracking-tight">Smart Home Dashboard</h1>
          </div>
          <div className="flex flex-wrap items-center gap-3">
            <Button variant="outline" size="sm" onClick={onLogsClick}>
              <ScrollText className="mr-2 h-4 w-4" />
              Logs
            </Button>
            <Button variant="outline" size="sm" onClick={onLogout}>
              <LogOut className="mr-2 h-4 w-4" />
              Logout
            </Button>
            <Badge variant={mqttConnected ? "default" : "destructive"} className="px-3 py-1 text-sm">
              <Wifi className="mr-2 h-4 w-4" />
              {mqttConnected ? "MQTT Connected" : "MQTT Disconnected"}
            </Badge>
            {mqttError ? (
              <Badge variant="destructive" className="px-3 py-1 text-sm">
                MQTT Error
              </Badge>
            ) : null}
          </div>
        </div>

        <div className="grid gap-4 md:grid-cols-2 xl:grid-cols-5">
          <MetricCard title="Temperature" value={temperature} unit="°C" icon={Thermometer} />
          <MetricCard title="Humidity" value={humidity} unit="%" icon={Droplets} />
          <MetricCard title="Light Intensity" value={lightIntensity} unit="lx" icon={Sun} />
          <MetricCard title="Air Pressure" value={pressure} unit="hPa" icon={Gauge} />
          <Card className="rounded-2xl shadow-sm">
            <CardContent className="p-5">
              <div className="flex items-start justify-between">
                <div>
                  <p className="text-sm text-slate-500">Soil Moisture</p>
                  <div className="mt-2 flex items-end gap-1">
                    <span className="text-3xl font-semibold">{soilMoisture.toFixed(1)}</span>
                    <span className="pb-1 text-sm text-slate-500">%</span>
                  </div>
                </div>
                <div className="rounded-2xl bg-slate-100 p-3">
                  <Droplets className="h-5 w-5" />
                </div>
              </div>
              <Button
                variant="outline"
                className="mt-3 w-full"
                onClick={publishPumpOn}
                disabled={!mqttConnected || moduleStatus.Meteostanica !== "ONLINE"}>
                Watering
              </Button>
            </CardContent>
          </Card>
        </div>

        <Tabs defaultValue="overview" className="space-y-4">
          <TabsList className="grid w-full grid-cols-4 rounded-2xl bg-white p-1 shadow-sm">
            <TabsTrigger value="overview">Overview</TabsTrigger>
            <TabsTrigger value="gas">Gas</TabsTrigger>
            <TabsTrigger value="security">Security</TabsTrigger>
            <TabsTrigger value="windowshade">Windowshade</TabsTrigger>
          </TabsList>

          <TabsContent value="overview" className="space-y-4">
            <div className="grid gap-4 lg:grid-cols-3">
              <Card className="rounded-2xl shadow-sm lg:col-span-2">
                <CardHeader>
                  <CardTitle>Sensor Data Timeline</CardTitle>
                </CardHeader>
                <CardContent className="h-80">
                  <div className="mb-3 text-sm text-slate-500">
                    History rows: {historyData.length} / Loading: {String(historyLoading)}
                  </div>
                  {historyLoading ? (
                    <div className="flex h-full items-center justify-center text-slate-500">Loading history chart...</div>
                  ) : historyData.length === 0 ? (
                    <div className="flex h-full items-center justify-center text-slate-500">No history data available.</div>
                  ) : (
                    <ResponsiveContainer width="100%" height="100%">
                      <LineChart data={historyData} margin={{ top: 10, right: 10, bottom: 5, left: 0 }}>
                        <CartesianGrid strokeDasharray="3 3" />
                        <XAxis dataKey="timestamp" tickFormatter={formatTimestamp} />
                        <YAxis />
                        <Tooltip labelFormatter={(value) => formatTimestamp(String(value))} />
                        <Legend verticalAlign="top" height={36} />
                        <Line type="monotone" dataKey="temp" name="Temperature" stroke="#1f77b4" strokeWidth={2} dot={false} connectNulls />
                        <Line type="monotone" dataKey="humidity" name="Humidity" stroke="#ff7f0e" strokeWidth={2} dot={false} connectNulls />
                        <Line type="monotone" dataKey="soil" name="Soil Moisture" stroke="#2ca02c" strokeWidth={2} dot={false} connectNulls />
                        <Line type="monotone" dataKey="light" name="Light" stroke="#d62728" strokeWidth={2} dot={false} connectNulls />
                      </LineChart>
                    </ResponsiveContainer>
                  )}
                </CardContent>
              </Card>

              <Card className="rounded-2xl shadow-sm lg:col-span-1">
                <CardHeader>
                  <CardTitle>Modules</CardTitle>
                </CardHeader>
                <CardContent className="space-y-4">
                  {([
                    ["Module_Meteostanica", moduleStatus.Meteostanica],
                    ["Module_Display", moduleStatus.Display],
                    ["Module_COSensor", moduleStatus.COSensor],
                    ["Module_Security", moduleStatus.Security],
                    ["Module_Windowshade", moduleStatus.Windowshade],
                  ] as [string, string][]).map(([name, status]) => (
                    <div key={name} className="rounded-2xl border p-4">
                      <div className="flex items-center justify-between">
                        <p className="font-medium">{name}</p>
                        <Badge className={status === "ONLINE" ? "bg-green-500 hover:bg-green-500 text-white border-0" : "bg-red-500 hover:bg-red-500 text-white border-0"}>
                          {status}
                        </Badge>
                      </div>
                    </div>
                  ))}
                </CardContent>
              </Card>
            </div>
          </TabsContent>

          <TabsContent value="security" className="space-y-4">
            <div className="grid gap-4 lg:grid-cols-1">
              <Card className={`rounded-2xl shadow-sm ${moduleStatus.Security !== "ONLINE" ? "opacity-60" : ""}`}>
                <CardHeader>
                  <CardTitle className="flex items-center justify-between">
                    <span className="flex items-center gap-2"><ShieldAlert className="h-5 w-5" /> Security Module</span>
                    <Badge className={moduleStatus.Security === "ONLINE" ? "bg-green-500 hover:bg-green-500 text-white border-0" : "bg-red-500 hover:bg-red-500 text-white border-0"}>
                      {moduleStatus.Security}
                    </Badge>
                  </CardTitle>
                </CardHeader>
                <CardContent className="space-y-4">
                  <div className={`grid gap-4 sm:grid-cols-2 ${moduleStatus.Security !== "ONLINE" ? "pointer-events-none" : ""}`}>
                    {/* Kártyaszám panel */}
                    <div className="flex items-center gap-4 rounded-2xl border p-4">
                      <div className="rounded-2xl bg-slate-100 p-3">
                        <CreditCard className="h-5 w-5" />
                      </div>
                      <div className="flex-1">
                        <p className="text-sm text-slate-500">Registered Cards</p>
                        <div className="flex items-center gap-2 mt-1">
                          <p className="text-2xl font-semibold">
                            {authorizedCardCount === null ? "…" : authorizedCardCount}
                          </p>
                          <Button size="sm" variant="outline" onClick={fetchCardCount} className="ml-auto">
                            Refresh
                          </Button>
                        </div>
                      </div>
                    </div>
                    {/* Enroll mode panel */}
                    <div className="flex items-center gap-4 rounded-2xl border p-4">
                      <div className="rounded-2xl bg-slate-100 p-3">
                        <UserPlus className="h-5 w-5" />
                      </div>
                      <div className="flex-1">
                        <p className="text-sm text-slate-500">Enroll New Card</p>
                        <p className="text-xs text-slate-400 mb-2">Tap a card on the reader to register it</p>
                        <Button
                          size="sm"
                          onClick={handleEnrollMode}
                          disabled={enrollLoading || !mqttConnected || moduleStatus.Security !== "ONLINE"}
                        >
                          {enrollLoading ? "Activating…" : "Start Enroll Mode"}
                        </Button>
                      </div>
                    </div>
                  </div>
                  <div className={`flex items-center justify-between rounded-2xl border p-4 ${moduleStatus.Security !== "ONLINE" ? "pointer-events-none" : ""}`}>
                    <div>
                      <p className="font-medium">Alarm Active</p>
                      <p className="text-sm text-slate-500">PIR + magnetic sensors</p>
                    </div>
                    <span className={`rounded-full px-3 py-1 text-sm font-semibold ${alarmArmed ? "bg-red-100 text-red-600" : "bg-green-100 text-green-600"}`}>
                      {alarmArmed ? "Armed" : "Disarmed"}
                    </span>
                  </div>
                </CardContent>
              </Card>
            </div>
          </TabsContent>

          <TabsContent value="gas" className="space-y-4">
            <div className="grid gap-4 lg:grid-cols-2">
              <Card className={`rounded-2xl shadow-sm ${moduleStatus.COSensor !== "ONLINE" ? "opacity-60" : ""}`}>
                <CardHeader>
                  <CardTitle className="flex items-center justify-between">
                    <span className="flex items-center gap-2"> <AlertTriangle className="h-5 w-5" /> CO Module </span>
                    <Badge className={moduleStatus.COSensor === "ONLINE" ? "bg-green-500 hover:bg-green-500 text-white border-0" : "bg-red-500 hover:bg-red-500 text-white border-0"}>
                      {moduleStatus.COSensor}
                    </Badge>
                  </CardTitle>
                </CardHeader>
                <CardContent className={`space-y-3 text-sm text-slate-500 ${moduleStatus.COSensor !== "ONLINE" ? "pointer-events-none" : ""}`}>
                  <div className="flex items-center justify-between">
                    <div>
                      <p className="font-medium">Status and last siren signal</p>
                    </div>
                  </div>
                  <div>Mode: {modeState}</div>
                  <div>Relay: {relayState}</div>
                  <div>Air Quality: {airQualityData ?? "-"}</div>
                  <div>Siren: {sirenState}</div>
                </CardContent>
              </Card>
              <Card className={`rounded-2xl shadow-sm ${moduleStatus.COSensor !== "ONLINE" ? "opacity-60" : ""}`}>
                <CardHeader>
                  <CardTitle className="flex items-center justify-between">
                    <span>Ventilation Relay</span>
                    <Badge className={moduleStatus.COSensor === "ONLINE" ? "bg-green-500 hover:bg-green-500 text-white border-0" : "bg-red-500 hover:bg-red-500 text-white border-0"}>
                      {moduleStatus.COSensor}
                    </Badge>
                  </CardTitle>
                </CardHeader>
                <CardContent className={`space-y-4 ${moduleStatus.COSensor !== "ONLINE" ? "pointer-events-none" : ""}`}>
                  <div className="space-y-3 text-sm text-slate-500">
                    <div>Mode and relay commands — AUTO / MANUAL ventilation control</div>
                    <div>Mode: {modeState}</div>
                    <div>Relay: {relayState}</div>
                  </div>
                  <div className="flex flex-wrap gap-2">
                    <Button
                      variant={relayState === "ON" && modeState === "MANUAL" ? "default" : "outline"}
                      onClick={publishRelayOn}
                      disabled={!mqttConnected || moduleStatus.COSensor !== "ONLINE"}
                    >
                      Ventilation On
                    </Button>
                    <Button
                      variant={relayState === "OFF" && modeState === "MANUAL" ? "default" : "outline"}
                      onClick={publishRelayOff}
                      disabled={!mqttConnected || moduleStatus.COSensor !== "ONLINE"}
                    >
                      Ventilation Off
                    </Button>
                    <Button
                      variant={modeState === "AUTO" ? "default" : "outline"}
                      onClick={publishModeAuto}
                      disabled={!mqttConnected || moduleStatus.COSensor !== "ONLINE"}
                    >
                      Reset to AUTO
                    </Button>
                  </div>
                </CardContent>
              </Card>
            </div>
          </TabsContent>

          <TabsContent value="windowshade" className="space-y-4">
            <div className="grid gap-4 lg:grid-cols-1">
              <Card className={`rounded-2xl shadow-sm ${moduleStatus.Windowshade !== "ONLINE" ? "opacity-60" : ""}`}>
                <CardHeader>
                  <CardTitle className="flex items-center justify-between">
                    <span className="flex items-center gap-2"><Gauge className="h-5 w-5" /> Windowshade Module</span>
                    <Badge className={moduleStatus.Windowshade === "ONLINE" ? "bg-green-500 hover:bg-green-500 text-white border-0" : "bg-red-500 hover:bg-red-500 text-white border-0"}>
                      {moduleStatus.Windowshade}
                    </Badge>
                  </CardTitle>
                </CardHeader>
                <CardContent className="space-y-4 text-sm text-slate-500">
                  <div>State: {windowshadeState}</div>
                  <div>Position: {windowshadePosition}</div>
                  <div>MQTT status: {commandStatus || "Unknown"}</div>
                  <div className="flex flex-wrap gap-2">
                    <Button
                      variant="outline"
                      onClick={() => publishCommand("CLOSE")}
                      disabled={!mqttConnected || moduleStatus.Windowshade !== "ONLINE"}
                    >
                      Close
                    </Button>
                    <Button
                      variant="outline"
                      onClick={() => publishCommand("PARTIAL")}
                      disabled={!mqttConnected || moduleStatus.Windowshade !== "ONLINE"}
                    >
                      Partial
                    </Button>
                    <Button
                      variant="outline"
                      onClick={() => publishCommand("OPEN")}
                      disabled={!mqttConnected || moduleStatus.Windowshade !== "ONLINE"}
                    >
                      Open
                    </Button>
                  </div>
                  <div className="rounded-2xl border p-4 space-y-3">
                    <div>
                      <p className="font-medium text-slate-800">Windowshade Settings </p>
                      <p className="text-xs text-slate-500">Enter OPEN and CLOSE timeslots in the format: (HH:MM).</p>
                    </div>
                    {windowshadeSettingsLoading ? (
                      <div className="text-xs text-slate-500">Loading settings...</div>
                    ) : (
                      <>
                        <div className="grid gap-3 sm:grid-cols-2">
                          <div className="space-y-1">
                            <label className="text-xs font-medium text-slate-700">OPEN (opening time)</label>
                            <Input
                              type="time"
                              value={windowshadeOpenValue}
                              onChange={(event) => setWindowshadeOpenValue(event.target.value)}
                              step={60}
                            />
                          </div>
                          <div className="space-y-1">
                            <label className="text-xs font-medium text-slate-700">CLOSE (closing time)</label>
                            <Input
                              type="time"
                              value={windowshadeCloseValue}
                              onChange={(event) => setWindowshadeCloseValue(event.target.value)}
                              step={60}
                            />
                          </div>
                        </div>
                        <div className="flex items-center gap-2">
                          <Button
                            variant="default"
                            onClick={handleSaveWindowshadeSettings}
                            disabled={windowshadeSettingsSaving}
                          >
                            {windowshadeSettingsSaving ? "Mentes..." : "Save times"}
                          </Button>
                          {windowshadeSettingsMessage ? (
                            <span className="text-xs text-slate-600">{windowshadeSettingsMessage}</span>
                          ) : null}
                        </div>
                      </>
                    )}
                  </div>
                </CardContent>
              </Card>
            </div>
          </TabsContent>

        </Tabs>
      </div>
    </div>
  );
}