import { useEffect, useState, useCallback } from 'react';
import mqtt from 'mqtt';

export interface MqttMessage {
  topic: string;
  payload: any;
}

export function useMqtt() {
  const [client, setClient] = useState<mqtt.MqttClient | null>(null);
  const [messages, setMessages] = useState<MqttMessage[]>([]);
  const [status, setStatus] = useState<'connecting' | 'connected' | 'error' | 'offline'>('connecting');

  useEffect(() => {
    // Connect to the MQTT broker via WebSockets on port 9001
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    // Use the same host but fixed port 9001 for Mosquitto WebSockets
    const brokerUrl = `${protocol}//${window.location.hostname}:9001`;
    
    console.log('Connecting to MQTT at:', brokerUrl);
    
    const mqttClient = mqtt.connect(brokerUrl, {
      reconnectPeriod: 1000,
      connectTimeout: 30 * 1000,
    });

    mqttClient.on('connect', () => {
      console.log('Connected to MQTT broker');
      setStatus('connected');
      mqttClient.subscribe('ride/#');
    });

    mqttClient.on('message', (topic, payload) => {
      try {
        const data = JSON.parse(payload.toString());
        setMessages((prev) => [{ topic, payload: data }, ...prev].slice(0, 50));
      } catch (e) {
        console.warn('Received non-JSON payload on topic:', topic);
      }
    });

    mqttClient.on('error', (err) => {
      console.error('MQTT error:', err);
      setStatus('error');
    });

    mqttClient.on('offline', () => {
      setStatus('offline');
    });

    setClient(mqttClient);

    return () => {
      mqttClient.end();
    };
  }, []);

  const publish = useCallback((topic: string, message: any) => {
    if (client && client.connected) {
      client.publish(topic, JSON.stringify(message));
    }
  }, [client]);

  return { client, messages, status, publish };
}
