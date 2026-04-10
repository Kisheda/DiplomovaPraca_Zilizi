import { useEffect, useState } from "react";
import { supabase } from "@/lib/supabase";

export interface HistoryDataPoint {
  timestamp: string;
  time: string;
  temp: number;
  humidity: number;
  light: number;
  pressure: number;
  soil: number;
}

export const useSupabaseData = () => {
  const [historyData, setHistoryData] = useState<HistoryDataPoint[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const fetchHistory = async () => {
      try {
        setLoading(true);

        const since = new Date(Date.now() - 24 * 60 * 60 * 1000).toISOString();
        const { data: measurements, error: measurementsError } = await supabase
          .from("Measurements")
          .select("created_at,measurement_type,measurement_data")
          .gte("created_at", since)
          .order("created_at", { ascending: true });

        console.debug("Supabase raw measurements count:", measurements?.length, measurements?.slice(0, 10));

        if (measurementsError) {
          throw new Error(`Measurements Error: ${measurementsError.message}`);
        }

        const measurementsByTimestamp: Record<string, HistoryDataPoint> = {};

        measurements?.forEach((record) => {
          if (!record.measurement_type || !record.created_at) return;
          const type = record.measurement_type.toString().toLowerCase();
          if (!["temperature", "humidity", "light", "pressure", "soil"].includes(type)) return;
          const date = new Date(record.created_at);
          const timestamp = date.toISOString();
          const time = date.toLocaleTimeString("hu-HU", { hour: "2-digit", minute: "2-digit", hour12: false });

          if (!measurementsByTimestamp[timestamp]) {
            measurementsByTimestamp[timestamp] = {
              timestamp,
              time,
              temp: 0,
              humidity: 0,
              light: 0,
              pressure: 0,
              soil: 0,
            };
          }

          switch (type) {
            case "temperature":
              measurementsByTimestamp[timestamp].temp = record.measurement_data;
              break;
            case "humidity":
              measurementsByTimestamp[timestamp].humidity = record.measurement_data;
              break;
            case "light":
              measurementsByTimestamp[timestamp].light = record.measurement_data;
              break;
            case "pressure":
              measurementsByTimestamp[timestamp].pressure = record.measurement_data;
              break;
            case "soil":
              measurementsByTimestamp[timestamp].soil = record.measurement_data;
              break;
          }
        });

        const sortedHistory = Object.values(measurementsByTimestamp).sort((a, b) => a.timestamp.localeCompare(b.timestamp));
        console.debug("Supabase history rows:", sortedHistory.length, sortedHistory.slice(0, 10));
        setHistoryData(sortedHistory);
        setError(null);
      } catch (err) {
        const errorMessage = err instanceof Error ? err.message : "Unknown error";
        setError(errorMessage);
        console.error("History fetch error:", err);
      } finally {
        setLoading(false);
      }
    };

    fetchHistory();
  }, []);

  return { historyData, loading, error };
};