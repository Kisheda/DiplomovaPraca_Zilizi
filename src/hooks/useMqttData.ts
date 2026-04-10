import { useEffect, useRef, useState } from "react";
import mqtt from "mqtt";

const MQTT_BROKER_URL = import.meta.env.VITE_MQTT_BROKER_URL ?? "wss://b583e186639e4bc2aa7bc95605777610.s1.eu.hivemq.cloud:8884/mqtt";
const MQTT_USERNAME = import.meta.env.VITE_MQTT_USERNAME ?? "WebPage";
const MQTT_PASSWORD = import.meta.env.VITE_MQTT_PASSWORD ?? "Asd123456789";

const TOPICS = {
  soil: "meteostanica/soil",
  temperature: "meteostanica/temperature",
  humidity: "meteostanica/humidity",
  light: "meteostanica/light",
  pressure: "meteostanica/pressure",
  pumpState: "meteostanica/pump",
  meteoStatus: "meteostanica/status",
  coStatus: "modul_cosensor/status",
  airQuality: "modul_cosensor/airquality",
  sirenState: "modul_cosensor/siren",
  relayState: "modul_cosensor/relay/state",
  modeState: "modul_cosensor/mode/state",
  windowshadeState: "windowshade/state",
  windowshadePosition: "windowshade/position",
  windowshadeStatus: "windowshade/status",
  displayStatus: "modul_display/status",
  securityStatus: "modul_security/state",
  alarm: "modul_security/alarm",
};

const COMMAND_TOPIC = "windowshade/cmd";
const RELAY_SET_TOPIC = "modul_cosensor/relay/set";
const MODE_SET_TOPIC = "modul_cosensor/mode/set";
const PUMP_SET_TOPIC = "meteostanica/pump/set";
const SECURITY_ENROLL_SET_TOPIC = "modul_security/enroll/set";

const parseNumberPayload = (payload: Uint8Array | string): number => {
  const raw = payload instanceof Uint8Array ? new TextDecoder().decode(payload) : String(payload);
  const value = parseFloat(raw.replace(",", "."));
  return Number.isFinite(value) ? value : 0;
};

const payloadToString = (payload: Uint8Array | string): string => {
  return payload instanceof Uint8Array ? new TextDecoder().decode(payload) : String(payload);
};

export interface MqttSensorData {
  temperature: number;
  humidity: number;
  lightIntensity: number;
  pressure: number;
  soilMoisture: number;
  connected: boolean;
  error: string | null;
  modeState: string;
  relayState: string;
}

export const useMqttData = () => {
  const [temperature, setTemperature] = useState(0);
  const [humidity, setHumidity] = useState(0);
  const [lightIntensity, setLightIntensity] = useState(0);
  const [pressure, setPressure] = useState(0);
  const [soilMoisture, setSoilMoisture] = useState(0);
  const [pumpState, setPumpState] = useState<string>("Unknown");
  const [coStatus, setCoStatus] = useState<string>("Unknown");
  const [meteoStatus, setMeteoStatus] = useState<string>("Unknown");
  const [windowshadeState, setWindowshadeState] = useState<string>("Unknown");
  const [windowshadePosition, setWindowshadePosition] = useState<string>("--");
  const [windowshadeStatus, setWindowshadeStatus] = useState<string>("Unknown");
  const [displayStatus, setDisplayStatus] = useState<string>("Unknown");
  const [securityStatus, setSecurityStatus] = useState<string>("Unknown");
  const [sirenState, setSirenState] = useState<string>("Unknown");
  const [alarmState, setAlarmState] = useState<string>("Unknown");
  const [airQualityData, setAirQualityData] = useState<number | null>(null);
  const [connected, setConnected] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [commandStatus, setCommandStatus] = useState<string>("Unknown");
  const [modeState, setModeState] = useState<string>("Unknown");
  const [relayState, setRelayState] = useState<string>("Unknown");
  const clientRef = useRef<mqtt.MqttClient | null>(null);

  useEffect(() => {
    const client = mqtt.connect(MQTT_BROKER_URL, {
      clientId: `webpage-${Math.random().toString(16).slice(2)}`,
      username: MQTT_USERNAME,
      password: MQTT_PASSWORD,
      keepalive: 30,
      clean: true,
      reconnectPeriod: 5000,
      connectTimeout: 10000,
    });
    clientRef.current = client;

    const handleMessage = (topic: string, payload: Uint8Array) => {
      const value = parseNumberPayload(payload);

      switch (topic) {
        case TOPICS.temperature:
          setTemperature(value);
          break;
        case TOPICS.humidity:
          setHumidity(value);
          break;
        case TOPICS.light:
          setLightIntensity(value);
          break;
        case TOPICS.pressure:
          setPressure(value);
          break;
        case TOPICS.soil:
          setSoilMoisture(value);
          break;
        case TOPICS.pumpState:
          setPumpState(payloadToString(payload));
          break;
        case TOPICS.meteoStatus:
          setMeteoStatus(payloadToString(payload));
          break;
        case TOPICS.coStatus:
          setCoStatus(payloadToString(payload));
          break;
        case TOPICS.airQuality: {
          const rawValue = parseNumberPayload(payload);
          setAirQualityData(rawValue);
          break;
        }
        case TOPICS.sirenState:
          setSirenState(payloadToString(payload));
          break;
        case TOPICS.modeState:
          setModeState(payloadToString(payload));
          break;
        case TOPICS.relayState:
          setRelayState(payloadToString(payload));
          break;
        case TOPICS.windowshadeState:
          setWindowshadeState(payloadToString(payload));
          break;
        case TOPICS.windowshadePosition:
          setWindowshadePosition(payloadToString(payload));
          break;
        case TOPICS.windowshadeStatus:
          setWindowshadeStatus(payloadToString(payload));
          break;
        case TOPICS.displayStatus:
          setDisplayStatus(payloadToString(payload));
          break;
        case TOPICS.securityStatus:
          setSecurityStatus(payloadToString(payload));
          break;
        case TOPICS.alarm:
          setAlarmState(payloadToString(payload));
          break;
      }
    };

    client.on("connect", () => {
      setConnected(true);
      setError(null);
      client.subscribe(Object.values(TOPICS), (err: Error | null) => {
        if (err) {
          setError(`MQTT subscribe error: ${err.message}`);
        }
      });
    });

    client.on("reconnect", () => {
      setConnected(false);
    });

    client.on("close", () => {
      setConnected(false);
    });

    client.on("error", (err: Error) => {
      setError(`MQTT error: ${err.message}`);
      setConnected(false);
    });

    client.on("message", (topic: string, payload: Uint8Array) => {
      handleMessage(topic, payload);
    });

    return () => {
      client.end(true);
    };
  }, []);

  const publishCommand = (command: string) => {
    if (!clientRef.current || !connected) {
      setError("MQTT not connected");
      return;
    }

    clientRef.current.publish(COMMAND_TOPIC, command, { qos: 1 }, (err) => {
      if (err) {
        setError(`MQTT publish error: ${err.message}`);
      }
    });

    setCommandStatus(command);
  };

  const publishRelayOn = () => {
    if (!clientRef.current || !connected) {
      setError("MQTT not connected");
      return;
    }
    clientRef.current.publish(MODE_SET_TOPIC, "MANUAL", { qos: 1 });
    clientRef.current.publish(RELAY_SET_TOPIC, "ON", { qos: 1 });
    setRelayState("ON");
    setModeState("MANUAL");
  };

  const publishRelayOff = () => {
    if (!clientRef.current || !connected) {
      setError("MQTT not connected");
      return;
    }
    clientRef.current.publish(MODE_SET_TOPIC, "MANUAL", { qos: 1 });
    clientRef.current.publish(RELAY_SET_TOPIC, "OFF", { qos: 1 });
    setRelayState("OFF");
    setModeState("MANUAL");
  };

  const publishModeAuto = () => {
    if (!clientRef.current || !connected) {
      setError("MQTT not connected");
      return;
    }
    clientRef.current.publish(MODE_SET_TOPIC, "AUTO", { qos: 1 });
    clientRef.current.publish(RELAY_SET_TOPIC, "OFF", { qos: 1 });
    setModeState("AUTO");
    setRelayState("OFF");
  };

  const publishPumpOn = () => {
    if (!clientRef.current || !connected) {
      setError("MQTT not connected");
      return;
    }
    clientRef.current.publish(PUMP_SET_TOPIC, "ON", { qos: 1 });
    setPumpState("ON");
    setTimeout(() => {
      if (clientRef.current) {
        clientRef.current.publish(PUMP_SET_TOPIC, "OFF", { qos: 1 });
        setPumpState("OFF");
      }
    }, 3000);
  };

  const publishPumpOff = () => {
    if (!clientRef.current || !connected) {
      setError("MQTT not connected");
      return;
    }
    clientRef.current.publish(PUMP_SET_TOPIC, "OFF", { qos: 1 });
    setPumpState("OFF");
  };

  return {
    temperature,
    humidity,
    lightIntensity,
    pressure,
    soilMoisture,
    pumpState,
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
    connected,
    error,
    commandStatus,
    publishCommand,
    modeState,
    relayState,
    publishRelayOn,
    publishRelayOff,
    publishModeAuto,
    publishPumpOn,
    publishPumpOff,
    publishEnrollOn,
  };

  function publishEnrollOn() {
    if (!clientRef.current || !connected) return;
    clientRef.current.publish(SECURITY_ENROLL_SET_TOPIC, "ON", { qos: 1 });
  }
};
