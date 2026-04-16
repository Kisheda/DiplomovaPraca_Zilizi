import { useEffect, useMemo, useState } from "react";
import { Card, CardContent } from "@/components/ui/card";
import { Button } from "@/components/ui/button";
import { Badge } from "@/components/ui/badge";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { ArrowLeft, Check, Pencil, RefreshCw, Trash2, X } from "lucide-react";
import { supabase, MeasurementRecord } from "@/lib/supabase";

interface LogsPageProps {
  onBack: () => void;
}

interface LogRecord {
  id: number;
  created_at: string;
  [key: string]: unknown;
}

type DatePreset = "all" | "today" | "yesterday" | "custom";

const formatDate = (iso: string) => {
  try {
    return new Date(iso).toLocaleString("en-GB", {
      year: "numeric",
      month: "2-digit",
      day: "2-digit",
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
      hour12: false,
    });
  } catch {
    return iso;
  }
};

const todayISO = () => new Date().toISOString().slice(0, 10);
const yesterdayISO = () => {
  const d = new Date();
  d.setDate(d.getDate() - 1);
  return d.toISOString().slice(0, 10);
};

const inDateRange = (iso: string, preset: DatePreset, from: string, to: string): boolean => {
  if (preset === "all") return true;
  const d = iso.slice(0, 10);
  if (preset === "today") return d === todayISO();
  if (preset === "yesterday") return d === yesterdayISO();
  if (preset === "custom") {
    if (from && d < from) return false;
    if (to && d > to) return false;
    return true;
  }
  return true;
};

export default function LogsPage({ onBack }: LogsPageProps) {
  const [measurements, setMeasurements] = useState<MeasurementRecord[]>([]);
  const [logs, setLogs] = useState<LogRecord[]>([]);
  const [loadingMeasurements, setLoadingMeasurements] = useState(true);
  const [loadingLogs, setLoadingLogs] = useState(true);
  const [errorMeasurements, setErrorMeasurements] = useState<string | null>(null);
  const [errorLogs, setErrorLogs] = useState<string | null>(null);

  // --- Authorized Cards ---
  const [cards, setCards] = useState<Record<string, unknown>[]>([]);
  const [loadingCards, setLoadingCards] = useState(true);
  const [errorCards, setErrorCards] = useState<string | null>(null);
  const [deletingCardId, setDeletingCardId] = useState<number | null>(null);
  const [cardSearch, setCardSearch] = useState("");
  const [editingCardId, setEditingCardId] = useState<number | null>(null);
  const [editingNote, setEditingNote] = useState<string>("");
  const [editingColumn, setEditingColumn] = useState<string>("note");
  const [savingCardId, setSavingCardId] = useState<number | null>(null);
  const [limit, setLimit] = useState<10 | 97>(97);

  // --- Measurement filters ---
  const [mDatePreset, setMDatePreset] = useState<DatePreset>("all");
  const [mDateFrom, setMDateFrom] = useState("");
  const [mDateTo, setMDateTo] = useState("");
  const [mType, setMType] = useState("all");
  const [mSearch, setMSearch] = useState("");

  // --- Log filters ---
  const [lDatePreset, setLDatePreset] = useState<DatePreset>("all");
  const [lDateFrom, setLDateFrom] = useState("");
  const [lDateTo, setLDateTo] = useState("");
  const [lModule, setLModule] = useState("all");
  const [lSearch, setLSearch] = useState("");

  const fetchMeasurements = async (lim: number) => {
    setLoadingMeasurements(true);
    setErrorMeasurements(null);
    const { data, error } = await supabase
      .from("Measurements")
      .select("id, created_at, measurement_type, measurement_data")
      .order("created_at", { ascending: false })
      .limit(lim);
    if (error) setErrorMeasurements(error.message);
    else setMeasurements(data ?? []);
    setLoadingMeasurements(false);
  };

  const fetchLogs = async (lim: number) => {
    setLoadingLogs(true);
    setErrorLogs(null);
    const { data, error } = await supabase
      .from("Logs")
      .select("*")
      .order("created_at", { ascending: false })
      .limit(lim);
    if (error) setErrorLogs(error.message);
    else setLogs((data ?? []) as LogRecord[]);
    setLoadingLogs(false);
  };

  const fetchCards = async () => {
    setLoadingCards(true);
    setErrorCards(null);
    const { data, error } = await supabase
      .from("Authorized_cards")
      .select("*")
      .order("id", { ascending: true });
    if (error) setErrorCards(error.message);
    else setCards((data ?? []) as Record<string, unknown>[]);
    setLoadingCards(false);
  };

  const handleDeleteCard = async (id: number) => {
    setDeletingCardId(id);
    const { error } = await supabase.from("Authorized_cards").delete().eq("id", id);
    if (error) setErrorCards(error.message);
    else setCards((prev) => prev.filter((c) => c["id"] !== id));
    setDeletingCardId(null);
  };

  const handleEditNote = (id: number, col: string, currentNote: unknown) => {
    setEditingCardId(id);
    setEditingColumn(col);
    setEditingNote(String(currentNote ?? ""));
  };

  const handleSaveNote = async (id: number) => {
    setSavingCardId(id);
    const updatePayload = { [editingColumn]: editingNote };
    const { data, error } = await supabase
      .from("Authorized_cards")
      .update(updatePayload)
      .eq("id", id)
      .select();
    console.log("update result", { data, error, id, updatePayload });
    if (error) {
      setErrorCards(error.message);
    } else if (!data || data.length === 0) {
      setErrorCards("Update had no effect — check RLS policies or column name in Supabase.");
    } else {
      setCards((prev) => prev.map((c) => c["id"] === id ? { ...c, [editingColumn]: editingNote } : c));
      setEditingCardId(null);
    }
    setSavingCardId(null);
  };

  const handleLimitChange = (newLimit: 10 | 97) => {
    setLimit(newLimit);
    fetchMeasurements(newLimit);
    fetchLogs(newLimit);
  };

  useEffect(() => {
    fetchMeasurements(limit);
    fetchLogs(limit);
    fetchCards();
  }, []);

  // --- Derived filter options ---
  const measurementTypes = useMemo(
    () => ["all", ...Array.from(new Set(measurements.map((m) => m.measurement_type)))],
    [measurements]
  );

  const logModules = useMemo(
    () => ["all", ...Array.from(new Set(logs.map((l) => String(l["Modul"] ?? "")).filter(Boolean)))],
    [logs]
  );

  // --- Filtered data ---
  const filteredMeasurements = useMemo(() => {
    return measurements.filter((m) => {
      if (!inDateRange(m.created_at, mDatePreset, mDateFrom, mDateTo)) return false;
      if (mType !== "all" && m.measurement_type !== mType) return false;
      if (mSearch) {
        const hay = `${m.measurement_type} ${m.measurement_data} ${m.created_at}`.toLowerCase();
        if (!hay.includes(mSearch.toLowerCase())) return false;
      }
      return true;
    });
  }, [measurements, mDatePreset, mDateFrom, mDateTo, mType, mSearch]);

  const filteredLogs = useMemo(() => {
    return logs.filter((log) => {
      if (!inDateRange(log.created_at, lDatePreset, lDateFrom, lDateTo)) return false;
      if (lModule !== "all" && String(log["Modul"] ?? "") !== lModule) return false;
      if (lSearch) {
        const hay = JSON.stringify(log).toLowerCase();
        if (!hay.includes(lSearch.toLowerCase())) return false;
      }
      return true;
    });
  }, [logs, lDatePreset, lDateFrom, lDateTo, lModule, lSearch]);

  const DateFilter = ({
    preset, setPreset, from, setFrom, to, setTo,
  }: {
    preset: DatePreset; setPreset: (v: DatePreset) => void;
    from: string; setFrom: (v: string) => void;
    to: string; setTo: (v: string) => void;
  }) => (
    <div className="flex flex-wrap items-center gap-2">
      {(["all", "today", "yesterday", "custom"] as DatePreset[]).map((p) => (
        <Button
          key={p}
          variant={preset === p ? "default" : "outline"}
          size="sm"
          className="h-7 px-2 text-xs capitalize"
          onClick={() => setPreset(p)}
        >
          {p}
        </Button>
      ))}
      {preset === "custom" && (
        <>
          <input
            type="date"
            value={from}
            onChange={(e) => setFrom(e.target.value)}
            className="h-7 rounded-md border px-2 text-xs"
          />
          <span className="text-xs text-slate-400">–</span>
          <input
            type="date"
            value={to}
            onChange={(e) => setTo(e.target.value)}
            className="h-7 rounded-md border px-2 text-xs"
          />
        </>
      )}
    </div>
  );

  return (
    <div className="min-h-screen bg-slate-50 p-6">
      <div className="mx-auto max-w-screen-2xl space-y-6">
        <div className="flex items-center justify-between rounded-3xl bg-white p-6 shadow-sm">
          <div className="flex items-center gap-4">
            <Button variant="outline" size="sm" onClick={onBack}>
              <ArrowLeft className="mr-2 h-4 w-4" />
              Back
            </Button>
            <h1 className="text-2xl font-bold tracking-tight">Database Logs</h1>
            <div className="flex items-center gap-1 rounded-lg border p-1">
              <Button
                variant={limit === 10 ? "default" : "ghost"}
                size="sm"
                className="h-6 px-2 text-xs"
                onClick={() => handleLimitChange(10)}
              >
                Last 10
              </Button>
              <Button
                variant={limit === 97 ? "default" : "ghost"}
                size="sm"
                className="h-6 px-2 text-xs"
                onClick={() => handleLimitChange(97)}
              >
                Last 97
              </Button>
            </div>
          </div>
          <Button
            variant="outline"
            size="sm"
            onClick={() => { fetchMeasurements(limit); fetchLogs(limit); fetchCards(); }}
            disabled={loadingMeasurements || loadingLogs || loadingCards}
          >
            <RefreshCw className={`mr-2 h-4 w-4 ${loadingMeasurements || loadingLogs || loadingCards ? "animate-spin" : ""}`} />
            Refresh
          </Button>
        </div>

        <Tabs defaultValue="measurements" className="space-y-4">
          <TabsList className="grid w-full grid-cols-3 rounded-2xl bg-white p-1 shadow-sm">
            <TabsTrigger value="measurements">Measurements</TabsTrigger>
            <TabsTrigger value="logs">Logs</TabsTrigger>
            <TabsTrigger value="cards">Authorized Cards</TabsTrigger>
          </TabsList>

          {/* ── MEASUREMENTS TAB ── */}
          <TabsContent value="measurements" className="space-y-3">
            <Card className="rounded-2xl shadow-sm">
              <CardContent className="p-4 space-y-3">
                <div className="flex flex-wrap gap-3 items-end">
                  <div className="space-y-1">
                    <p className="text-xs font-medium text-slate-400">Date</p>
                    <DateFilter preset={mDatePreset} setPreset={setMDatePreset} from={mDateFrom} setFrom={setMDateFrom} to={mDateTo} setTo={setMDateTo} />
                  </div>
                  <div className="space-y-1">
                    <p className="text-xs font-medium text-slate-400">Type</p>
                    <select
                      value={mType}
                      onChange={(e) => setMType(e.target.value)}
                      className="h-7 rounded-md border px-2 text-xs bg-white"
                    >
                      {measurementTypes.map((t) => (
                        <option key={t} value={t}>{t === "all" ? "All types" : t}</option>
                      ))}
                    </select>
                  </div>
                  <div className="space-y-1 flex-1 min-w-[160px]">
                    <p className="text-xs font-medium text-slate-400">Search</p>
                    <div className="relative">
                      <input
                        type="text"
                        placeholder="Search..."
                        value={mSearch}
                        onChange={(e) => setMSearch(e.target.value)}
                        className="h-7 w-full rounded-md border px-2 pr-6 text-xs"
                      />
                      {mSearch && (
                        <button onClick={() => setMSearch("")} className="absolute right-1.5 top-1/2 -translate-y-1/2 text-slate-400 hover:text-slate-600">
                          <X className="h-3 w-3" />
                        </button>
                      )}
                    </div>
                  </div>
                  <span className="text-xs text-slate-400 self-end pb-0.5">{filteredMeasurements.length} record{filteredMeasurements.length !== 1 ? "s" : ""}</span>
                </div>
              </CardContent>
            </Card>

            {errorMeasurements && <p className="text-sm text-destructive">{errorMeasurements}</p>}
            {loadingMeasurements && <p className="text-sm text-slate-500 p-4">Loading...</p>}
            {!loadingMeasurements && filteredMeasurements.length === 0 && <p className="text-sm text-slate-500 p-4">No matching records.</p>}
            {!loadingMeasurements && filteredMeasurements.length > 0 && (
              <Card className="rounded-2xl shadow-sm">
                <CardContent className="p-0">
                  <div className="overflow-x-auto">
                    <table className="w-full text-sm">
                      <thead>
                        <tr className="border-b text-left text-slate-500">
                          <th className="px-6 py-3 font-medium">#</th>
                          <th className="px-6 py-3 font-medium">Timestamp</th>
                          <th className="px-6 py-3 font-medium">Type</th>
                          <th className="px-6 py-3 font-medium">Value</th>
                        </tr>
                      </thead>
                      <tbody>
                        {filteredMeasurements.map((m, i) => (
                          <tr key={m.id} className="border-b last:border-0 hover:bg-slate-50">
                            <td className="px-6 py-3 text-slate-400">{i + 1}</td>
                            <td className="px-6 py-3 text-slate-600">{formatDate(m.created_at)}</td>
                            <td className="px-6 py-3">
                              <Badge variant="outline" className="capitalize">{m.measurement_type}</Badge>
                            </td>
                            <td className="px-6 py-3 font-semibold">{m.measurement_data}</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                </CardContent>
              </Card>
            )}
          </TabsContent>

          {/* ── LOGS TAB ── */}
          <TabsContent value="logs" className="space-y-3">
            <Card className="rounded-2xl shadow-sm">
              <CardContent className="p-4 space-y-3">
                <div className="flex flex-wrap gap-3 items-end">
                  <div className="space-y-1">
                    <p className="text-xs font-medium text-slate-400">Date</p>
                    <DateFilter preset={lDatePreset} setPreset={setLDatePreset} from={lDateFrom} setFrom={setLDateFrom} to={lDateTo} setTo={setLDateTo} />
                  </div>
                  {logModules.length > 1 && (
                    <div className="space-y-1">
                      <p className="text-xs font-medium text-slate-400">Module</p>
                      <select
                        value={lModule}
                        onChange={(e) => setLModule(e.target.value)}
                        className="h-7 rounded-md border px-2 text-xs bg-white"
                      >
                        {logModules.map((m) => (
                          <option key={m} value={m}>{m === "all" ? "All modules" : m}</option>
                        ))}
                      </select>
                    </div>
                  )}
                  <div className="space-y-1 flex-1 min-w-[160px]">
                    <p className="text-xs font-medium text-slate-400">Search</p>
                    <div className="relative">
                      <input
                        type="text"
                        placeholder="Search..."
                        value={lSearch}
                        onChange={(e) => setLSearch(e.target.value)}
                        className="h-7 w-full rounded-md border px-2 pr-6 text-xs"
                      />
                      {lSearch && (
                        <button onClick={() => setLSearch("")} className="absolute right-1.5 top-1/2 -translate-y-1/2 text-slate-400 hover:text-slate-600">
                          <X className="h-3 w-3" />
                        </button>
                      )}
                    </div>
                  </div>
                  <span className="text-xs text-slate-400 self-end pb-0.5">{filteredLogs.length} record{filteredLogs.length !== 1 ? "s" : ""}</span>
                </div>
              </CardContent>
            </Card>

            {errorLogs && <p className="text-sm text-destructive">{errorLogs}</p>}
            {loadingLogs && <p className="text-sm text-slate-500 p-4">Loading...</p>}
            {!loadingLogs && filteredLogs.length === 0 && <p className="text-sm text-slate-500 p-4">No matching records.</p>}
            {!loadingLogs && filteredLogs.map((log, i) => {
              const fields = Object.entries(log).filter(([k]) => k !== "id" && k !== "created_at");
              return (
                <Card key={log.id} className="rounded-xl shadow-sm">
                  <CardContent className="p-3">
                    <div className="mb-2 flex items-center justify-between">
                      <span className="text-xs text-slate-400">#{i + 1}</span>
                      <span className="text-xs text-slate-400">{formatDate(log.created_at)}</span>
                    </div>
                    <div className="grid gap-1.5 grid-cols-2 md:grid-cols-4 lg:grid-cols-6 xl:grid-cols-8">
                      {fields.flatMap(([key, val]) => {
                        if (typeof val === "object" && val !== null && !Array.isArray(val)) {
                          return Object.entries(val as Record<string, unknown>).map(([subKey, subVal]) => (
                            <div key={`${key}_${subKey}`} className="rounded-lg bg-slate-50 px-2 py-1.5">
                              <p className="text-[10px] font-medium text-slate-400 capitalize leading-tight">{subKey.replace(/_/g, " ")}</p>
                              <p className="text-xs font-semibold text-slate-800 break-all">{String(subVal ?? "-")}</p>
                            </div>
                          ));
                        }
                        return [
                          <div key={key} className="rounded-lg bg-slate-50 px-2 py-1.5">
                            <p className="text-[10px] font-medium text-slate-400 capitalize leading-tight">{key.replace(/_/g, " ")}</p>
                            <p className="text-xs font-semibold text-slate-800 break-all">{String(val ?? "-")}</p>
                          </div>
                        ];
                      })}
                    </div>
                  </CardContent>
                </Card>
              );
            })}
          </TabsContent>
          {/* ── AUTHORIZED CARDS TAB ── */}
          <TabsContent value="cards" className="space-y-3">
            <Card className="rounded-2xl shadow-sm">
              <CardContent className="p-4">
                <div className="flex flex-wrap gap-3 items-end">
                  <div className="space-y-1 flex-1 min-w-[160px]">
                    <p className="text-xs font-medium text-slate-400">Search</p>
                    <div className="relative">
                      <input
                        type="text"
                        placeholder="Search..."
                        value={cardSearch}
                        onChange={(e) => setCardSearch(e.target.value)}
                        className="h-7 w-full rounded-md border px-2 pr-6 text-xs"
                      />
                      {cardSearch && (
                        <button onClick={() => setCardSearch("")} className="absolute right-1.5 top-1/2 -translate-y-1/2 text-slate-400 hover:text-slate-600">
                          <X className="h-3 w-3" />
                        </button>
                      )}
                    </div>
                  </div>
                  <span className="text-xs text-slate-400 self-end pb-0.5">
                    {cards.filter((c) => !cardSearch || JSON.stringify(c).toLowerCase().includes(cardSearch.toLowerCase())).length} card(s)
                  </span>
                </div>
              </CardContent>
            </Card>

            {errorCards && <p className="text-sm text-destructive">{errorCards}</p>}
            {loadingCards && <p className="text-sm text-slate-500 p-4">Loading...</p>}
            {!loadingCards && cards.length === 0 && <p className="text-sm text-slate-500 p-4">No authorized cards found.</p>}
            {!loadingCards && cards.length > 0 && (() => {
              const filteredCards = cards.filter((c) =>
                !cardSearch || JSON.stringify(c).toLowerCase().includes(cardSearch.toLowerCase())
              );
              const columns = Object.keys(cards[0]).filter((k) => k !== "id");
              return (
                <Card className="rounded-2xl shadow-sm">
                  <CardContent className="p-0">
                    <div className="overflow-x-auto">
                      <table className="w-full text-sm">
                        <thead>
                          <tr className="border-b text-left text-slate-500">
                            <th className="px-6 py-3 font-medium">#</th>
                            <th className="px-6 py-3 font-medium">ID</th>
                            {columns.map((col) => (
                              <th key={col} className="px-6 py-3 font-medium capitalize">{col.replace(/_/g, " ")}</th>
                            ))}
                            <th className="px-6 py-3 font-medium">Actions</th>
                          </tr>
                        </thead>
                        <tbody>
                          {filteredCards.map((card, i) => (
                            <tr key={String(card["id"])} className="border-b last:border-0 hover:bg-slate-50">
                              <td className="px-6 py-3 text-slate-400">{i + 1}</td>
                              <td className="px-6 py-3 text-slate-600">{String(card["id"])}</td>
                              {columns.map((col) => (
                                <td key={col} className="px-6 py-3">
                                  {col === "created_at" ? (
                                    formatDate(String(card[col] ?? ""))
                                  ) : col === "note" ? (
                                    editingCardId === Number(card["id"]) ? (
                                      <div className="flex items-center gap-1">
                                        <input
                                          type="text"
                                          value={editingNote}
                                          onChange={(e) => setEditingNote(e.target.value)}
                                          className="h-7 rounded-md border px-2 text-xs w-40"
                                          autoFocus
                                          onKeyDown={(e) => {
                                            if (e.key === "Enter") handleSaveNote(Number(card["id"]));
                                            if (e.key === "Escape") setEditingCardId(null);
                                          }}
                                        />
                                        <button
                                          onClick={() => handleSaveNote(Number(card["id"]))}
                                          disabled={savingCardId === Number(card["id"])}
                                          className="text-green-600 hover:text-green-800 disabled:opacity-50"
                                        >
                                          <Check className="h-4 w-4" />
                                        </button>
                                        <button
                                          onClick={() => setEditingCardId(null)}
                                          className="text-slate-400 hover:text-slate-600"
                                        >
                                          <X className="h-4 w-4" />
                                        </button>
                                      </div>
                                    ) : (
                                      <div className="flex items-center gap-1 group">
                                        <span>{String(card[col] ?? "-")}</span>
                                        <button
                                          onClick={() => handleEditNote(Number(card["id"]), col, card[col])}
                                          className="opacity-0 group-hover:opacity-100 text-slate-400 hover:text-slate-700 transition-opacity"
                                        >
                                          <Pencil className="h-3 w-3" />
                                        </button>
                                      </div>
                                    )
                                  ) : (
                                    String(card[col] ?? "-")
                                  )}
                                </td>
                              ))}
                              <td className="px-6 py-3">
                                <Button
                                  variant="outline"
                                  size="sm"
                                  className="h-7 px-2 text-xs text-red-600 border-red-200 hover:bg-red-50"
                                  disabled={deletingCardId === Number(card["id"])}
                                  onClick={() => handleDeleteCard(Number(card["id"]))}
                                >
                                  <Trash2 className="h-3 w-3 mr-1" />
                                  {deletingCardId === Number(card["id"]) ? "Deleting…" : "Delete"}
                                </Button>
                              </td>
                            </tr>
                          ))}
                        </tbody>
                      </table>
                    </div>
                  </CardContent>
                </Card>
              );
            })()}
          </TabsContent>
        </Tabs>
      </div>
    </div>
  );
}
