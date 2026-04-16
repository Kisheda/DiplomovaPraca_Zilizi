import { useEffect, useState } from "react";
import { supabase } from "@/lib/supabase";

export interface HistoryDataPoint {
  timestamp: string;
  time: string;
  temp: number | null;
  humidity: number | null;
  light: number | null;
  pressure: number | null;
  soil: number | null;
}

export const useSupabaseData = () => {
  const RECENT_RECORD_LIMIT = 97;
  const SENSOR_TYPES = ["temperature", "humidity", "light", "pressure", "soil"] as const;

  type SensorType = (typeof SENSOR_TYPES)[number];
  type MeasurementRow = {
    created_at: string;
    measurement_type: string;
    measurement_data: number;
  };

  const [historyData, setHistoryData] = useState<HistoryDataPoint[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [rangeLabel, setRangeLabel] = useState<string>("Last 97 records / sensor");
  const [rawMeasurementCount, setRawMeasurementCount] = useState<number>(0);

  useEffect(() => {
    const buildHistoryFromMeasurements = (measurementsByType: Record<SensorType, MeasurementRow[]>) => {
      const maxRows = Math.max(...SENSOR_TYPES.map((type) => measurementsByType[type].length), 0);

      return Array.from({ length: maxRows }, (_, index) => {
        const tempRow = measurementsByType.temperature[index];
        const humidityRow = measurementsByType.humidity[index];
        const lightRow = measurementsByType.light[index];
        const pressureRow = measurementsByType.pressure[index];
        const soilRow = measurementsByType.soil[index];
        const timestampSource = tempRow ?? humidityRow ?? lightRow ?? pressureRow ?? soilRow;
        const timestamp = timestampSource?.created_at ?? new Date().toISOString();

        return {
          timestamp,
          time: new Date(timestamp).toLocaleString("hu-HU", {
            month: "2-digit",
            day: "2-digit",
            hour: "2-digit",
            minute: "2-digit",
            second: "2-digit",
            hour12: false,
          }),
          temp: tempRow ? Number(tempRow.measurement_data) : null,
          humidity: humidityRow ? Number(humidityRow.measurement_data) : null,
          light: lightRow ? Number(lightRow.measurement_data) : null,
          pressure: pressureRow ? Number(pressureRow.measurement_data) : null,
          soil: soilRow ? Number(soilRow.measurement_data) : null,
        } satisfies HistoryDataPoint;
      }).sort((a, b) => a.timestamp.localeCompare(b.timestamp));
    };

    const fetchRecentMeasurements = async () => {
      const grouped: Record<SensorType, MeasurementRow[]> = {
        temperature: [],
        humidity: [],
        light: [],
        pressure: [],
        soil: [],
      };

      const results = await Promise.all(
        SENSOR_TYPES.map(async (type) => {
          const { data, error: measurementsError } = await supabase
            .from("Measurements")
            .select("created_at,measurement_type,measurement_data")
            .eq("measurement_type", type)
            .order("created_at", { ascending: false })
            .limit(RECENT_RECORD_LIMIT);

          if (measurementsError) {
            throw new Error(`${type} Error: ${measurementsError.message}`);
          }

          return { type, rows: (data ?? []).reverse() as MeasurementRow[] };
        })
      );

      results.forEach(({ type, rows }) => {
        grouped[type] = rows;
      });

      return grouped;
    };

    const fetchHistory = async () => {
      try {
        setLoading(true);

        const measurementsByType = await fetchRecentMeasurements();
        const sortedHistory = buildHistoryFromMeasurements(measurementsByType);

        if (sortedHistory.length === 0) {
          setHistoryData([]);
          setRawMeasurementCount(0);
          setRangeLabel(`Last ${RECENT_RECORD_LIMIT} records / sensor (no data)`);
          setError(null);
          return;
        }

        const validTimes = sortedHistory
          .map((row) => new Date(row.timestamp).getTime())
          .filter((timeMs) => Number.isFinite(timeMs));

        const windowStartMs = validTimes.length > 0 ? Math.min(...validTimes) : Date.now();
        const windowEndMs = validTimes.length > 0 ? Math.max(...validTimes) : Date.now();
        const totalRows = SENSOR_TYPES.reduce((sum, type) => sum + measurementsByType[type].length, 0);

        console.debug("Supabase history rows:", sortedHistory.length, sortedHistory.slice(0, 10));
        setHistoryData(sortedHistory);
        setRawMeasurementCount(totalRows);
        setRangeLabel(
          `Last ${RECENT_RECORD_LIMIT} records / sensor (${new Date(windowStartMs).toLocaleString("hu-HU", {
            month: "2-digit",
            day: "2-digit",
            hour: "2-digit",
            minute: "2-digit",
            hour12: false,
          })} → ${new Date(windowEndMs).toLocaleString("hu-HU", {
            month: "2-digit",
            day: "2-digit",
            hour: "2-digit",
            minute: "2-digit",
            hour12: false,
          })})`
        );
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
    const interval = setInterval(fetchHistory, 60000);

    return () => clearInterval(interval);
  }, []);

  return { historyData, loading, error, rangeLabel, rawMeasurementCount };
};