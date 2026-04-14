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
  const [historyData, setHistoryData] = useState<HistoryDataPoint[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [rangeLabel, setRangeLabel] = useState<string>("Last 24h / 15m");
  const [rawMeasurementCount, setRawMeasurementCount] = useState<number>(0);

  useEffect(() => {
    const buildHistoryFromMeasurements = (
      measurements: { created_at: string; measurement_type: string; measurement_data: number }[] | null | undefined,
      windowStartMs: number,
      windowEndMs: number
    ) => {
      const BUCKET_MS = 15 * 60 * 1000; // 15 minutes
      const measurementsByTimestamp: Record<string, HistoryDataPoint> = {};
      const bucketAggregates: Record<string, { temp: number[]; humidity: number[]; light: number[]; pressure: number[]; soil: number[] }> = {};

      const startBucketMs = Math.floor(windowStartMs / BUCKET_MS) * BUCKET_MS;
      const endBucketMs = Math.floor(windowEndMs / BUCKET_MS) * BUCKET_MS;

      for (let bucketTime = startBucketMs; bucketTime <= endBucketMs; bucketTime += BUCKET_MS) {
        const date = new Date(bucketTime);
        const timestamp = date.toISOString();
        measurementsByTimestamp[timestamp] = {
          timestamp,
          time: date.toLocaleTimeString("hu-HU", { hour: "2-digit", minute: "2-digit", hour12: false }),
          temp: null,
          humidity: null,
          light: null,
          pressure: null,
          soil: null,
        };

        bucketAggregates[timestamp] = {
          temp: [],
          humidity: [],
          light: [],
          pressure: [],
          soil: [],
        };
      }

      measurements?.forEach((record) => {
        if (!record.measurement_type || !record.created_at) return;
        const type = record.measurement_type.toString().toLowerCase();
        if (!["temperature", "humidity", "light", "pressure", "soil"].includes(type)) return;

        const rawTimeMs = new Date(record.created_at).getTime();
        if (!Number.isFinite(rawTimeMs)) return;
        if (rawTimeMs < windowStartMs || rawTimeMs > windowEndMs) return;
        const bucketTimeMs = Math.floor(rawTimeMs / BUCKET_MS) * BUCKET_MS;
        const bucketKey = new Date(bucketTimeMs).toISOString();

        if (!bucketAggregates[bucketKey]) return;

        const numericValue = Number(record.measurement_data);
        if (!Number.isFinite(numericValue)) return;

        switch (type) {
          case "temperature":
            bucketAggregates[bucketKey].temp.push(numericValue);
            break;
          case "humidity":
            bucketAggregates[bucketKey].humidity.push(numericValue);
            break;
          case "light":
            bucketAggregates[bucketKey].light.push(numericValue);
            break;
          case "pressure":
            bucketAggregates[bucketKey].pressure.push(numericValue);
            break;
          case "soil":
            bucketAggregates[bucketKey].soil.push(numericValue);
            break;
        }
      });

      const averageOrNull = (values: number[]) => {
        if (values.length === 0) return null;
        const avg = values.reduce((sum, value) => sum + value, 0) / values.length;
        return Number(avg.toFixed(2));
      };

      Object.keys(measurementsByTimestamp).forEach((timestamp) => {
        const aggregate = bucketAggregates[timestamp];
        measurementsByTimestamp[timestamp].temp = averageOrNull(aggregate.temp);
        measurementsByTimestamp[timestamp].humidity = averageOrNull(aggregate.humidity);
        measurementsByTimestamp[timestamp].light = averageOrNull(aggregate.light);
        measurementsByTimestamp[timestamp].pressure = averageOrNull(aggregate.pressure);
        measurementsByTimestamp[timestamp].soil = averageOrNull(aggregate.soil);
      });

      return Object.values(measurementsByTimestamp).sort((a, b) => a.timestamp.localeCompare(b.timestamp));
    };

    const fetchRecentMeasurements = async () => {
      const { data, error: measurementsError } = await supabase
        .from("Measurements")
        .select("created_at,measurement_type,measurement_data")
        .order("created_at", { ascending: false })
        .limit(5000);

      if (measurementsError) {
        throw new Error(`Measurements Error: ${measurementsError.message}`);
      }

      return (data ?? []).reverse();
    };

    const fetchHistory = async () => {
      try {
        setLoading(true);

        const allRecentMeasurements = await fetchRecentMeasurements();

        if (allRecentMeasurements.length === 0) {
          setHistoryData([]);
          setRawMeasurementCount(0);
          setRangeLabel("Last 24h / 15m (no data)");
          setError(null);
          return;
        }

        const latestMeasurementMs = new Date(allRecentMeasurements[allRecentMeasurements.length - 1].created_at).getTime();
        const windowEndMs = Number.isFinite(latestMeasurementMs) ? latestMeasurementMs : Date.now();
        const windowStartMs = windowEndMs - 24 * 60 * 60 * 1000;

        const measurements24h = allRecentMeasurements.filter((row) => {
          const rowMs = new Date(row.created_at).getTime();
          return Number.isFinite(rowMs) && rowMs >= windowStartMs && rowMs <= windowEndMs;
        });

        const sortedHistory = buildHistoryFromMeasurements(measurements24h, windowStartMs, windowEndMs);

        console.debug("Supabase history rows:", sortedHistory.length, sortedHistory.slice(0, 10));
        setHistoryData(sortedHistory);
        setRawMeasurementCount(measurements24h.length);
        setRangeLabel(
          `Last 24h / 15m (end: ${new Date(windowEndMs).toLocaleString("hu-HU", {
            year: "numeric",
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