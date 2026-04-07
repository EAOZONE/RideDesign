/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */

import React, { useState, useEffect, useMemo } from 'react';
import { 
  AlertCircle, 
  Activity, 
  Zap, 
  Settings, 
  RefreshCw, 
  Power, 
  Play, 
  Pause, 
  Radio, 
  Database, 
  Cpu, 
  Gauge, 
  Thermometer, 
  Wifi, 
  ChevronRight, 
  AlertTriangle 
} from 'lucide-react';
import { motion, AnimatePresence } from 'motion/react';
import { useMqtt } from './hooks/useMqtt';
import { TrackMap } from './components/TrackMap';
import { VideoFeed } from './components/VideoFeed';
import { clsx, type ClassValue } from 'clsx';
import { twMerge } from 'tailwind-merge';

function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

export default function App() {
  const { messages, status, publish } = useMqtt();
  const [rideMode, setRideMode] = useState<'manual' | 'auto'>('manual');
  const [isEstopActive, setIsEstopActive] = useState(false);
  const [sensors, setSensors] = useState<Record<string, number>>({
    Station1: 0, Station2: 0, Centry: 0, Switch1: 0, Switch2: 0,
    Rotate1: 0, Rotate2: 0, Basket: 0, Mid: 0, Drop1: 0, Drop2: 0
  });
  const [vehicles, setVehicles] = useState<Record<string, any>>({
    "0": { speed: 0, yaw: 0, pitch: 0 },
    "1": { speed: 0, yaw: 0, pitch: 0 }
  });

  // Process incoming MQTT messages
  useEffect(() => {
    if (messages.length === 0) return;
    const { topic, payload } = messages[0];

    if (topic.startsWith('ride/sensor/')) {
      const sensorId = topic.split('/')[2];
      setSensors(prev => ({ ...prev, [sensorId]: payload.state }));
    } else if (topic.startsWith('ride/vehicle/')) {
      const parts = topic.split('/');
      const vehicleId = parts[2];
      const type = parts[3]; // drive, servoYaw, servoPitch
      
      setVehicles(prev => {
        const current = prev[vehicleId] || {};
        if (type === 'drive') return { ...prev, [vehicleId]: { ...current, speed: payload.speed || 0 } };
        if (type === 'servoYaw') return { ...prev, [vehicleId]: { ...current, yaw: payload.angle || 0 } };
        if (type === 'servoPitch') return { ...prev, [vehicleId]: { ...current, pitch: payload.angle || 0 } };
        return prev;
      });
    } else if (topic === 'ride/system/estop') {
      setIsEstopActive(payload.active);
    } else if (topic === 'ride/system/mode') {
      setRideMode(payload.mode);
    }
  }, [messages]);

  const handleEstop = () => {
    const newState = !isEstopActive;
    publish('ride/system/estop', { active: newState });
    setIsEstopActive(newState);
  };

  const handleModeChange = (mode: 'manual' | 'auto') => {
    publish('ride/system/mode', { mode });
    setRideMode(mode);
  };

  const handleReset = () => {
    publish('ride/system/reset', {});
  };

  return (
    <div className="h-screen flex flex-col bg-black text-zinc-100 font-sans selection:bg-orange-500/30 overflow-hidden">
      {/* Top Navigation / Status Bar */}
      <header className="h-14 flex-none border-b border-zinc-800 bg-zinc-950/50 backdrop-blur-xl flex items-center justify-between px-6 z-50">
        <div className="flex items-center gap-4">
          <div className="w-10 h-10 bg-orange-500 rounded-lg flex items-center justify-center shadow-[0_0_20px_rgba(249,115,22,0.3)]">
            <Zap className="text-white fill-white" size={20} />
          </div>
          <div>
            <h1 className="text-sm font-bold uppercase tracking-[0.2em]">Ride Control System</h1>
            <div className="flex items-center gap-2">
              <div className={cn(
                "w-1.5 h-1.5 rounded-full",
                status === 'connected' ? "bg-green-500 animate-pulse" : "bg-red-500"
              )} />
              <span className="text-[10px] font-mono text-zinc-500 uppercase tracking-widest">
                MQTT: {status.toUpperCase()}
              </span>
            </div>
          </div>
        </div>

        <div className="flex items-center gap-6">
          <div className="flex items-center gap-2 px-3 py-1.5 bg-zinc-900 rounded-full border border-zinc-800">
            <Activity size={14} className="text-zinc-500" />
            <span className="text-[10px] font-mono text-zinc-400">CPU: 12%</span>
            <div className="w-px h-3 bg-zinc-800 mx-1" />
            <Wifi size={14} className="text-zinc-500" />
            <span className="text-[10px] font-mono text-zinc-400">RSSI: -64dBm</span>
          </div>
          
          <button 
            onClick={handleReset}
            className="flex items-center gap-2 px-4 py-2 bg-zinc-900 hover:bg-zinc-800 rounded-lg border border-zinc-800 transition-all text-[10px] font-mono uppercase tracking-widest"
          >
            <RefreshCw size={14} />
            Reset System
          </button>
        </div>
      </header>

      <main className="flex-1 overflow-hidden p-4 grid grid-cols-12 gap-4 max-w-[1800px] mx-auto w-full">
        
        {/* Left Column: Controls & Telemetry */}
        <div className="col-span-12 lg:col-span-4 flex flex-col gap-4 overflow-hidden">
          
          {/* System Mode Card */}
          <section className="bg-zinc-900/50 rounded-2xl border border-zinc-800 p-4 space-y-4 flex-none">
            <div className="flex justify-between items-center">
              <h2 className="text-[10px] font-mono text-zinc-500 uppercase tracking-widest flex items-center gap-2">
                <Settings size={14} /> System Mode
              </h2>
              <div className="px-2 py-1 bg-orange-500/10 rounded text-[10px] font-mono text-orange-500 uppercase">
                {rideMode}
              </div>
            </div>

            <div className="grid grid-cols-2 gap-2">
              <button 
                onClick={() => handleModeChange('manual')}
                className={cn(
                  "flex flex-col items-center gap-2 p-3 rounded-xl border transition-all group",
                  rideMode === 'manual' 
                    ? "bg-orange-500/10 border-orange-500/50 text-orange-500" 
                    : "bg-zinc-950 border-zinc-800 text-zinc-500 hover:border-zinc-700"
                )}
              >
                <div className={cn(
                  "w-8 h-8 rounded-full flex items-center justify-center transition-colors",
                  rideMode === 'manual' ? "bg-orange-500 text-white" : "bg-zinc-900 group-hover:bg-zinc-800"
                )}>
                  <Pause size={16} />
                </div>
                <span className="text-[9px] font-mono uppercase tracking-widest">Manual</span>
              </button>
              <button 
                onClick={() => handleModeChange('auto')}
                className={cn(
                  "flex flex-col items-center gap-2 p-3 rounded-xl border transition-all group",
                  rideMode === 'auto' 
                    ? "bg-green-500/10 border-green-500/50 text-green-500" 
                    : "bg-zinc-950 border-zinc-800 text-zinc-500 hover:border-zinc-700"
                )}
              >
                <div className={cn(
                  "w-8 h-8 rounded-full flex items-center justify-center transition-colors",
                  rideMode === 'auto' ? "bg-green-500 text-white" : "bg-zinc-900 group-hover:bg-zinc-800"
                )}>
                  <Play size={16} />
                </div>
                <span className="text-[9px] font-mono uppercase tracking-widest">Automatic</span>
              </button>
            </div>

            <button 
              onClick={handleEstop}
              className={cn(
                "w-full py-4 rounded-2xl flex flex-col items-center justify-center gap-2 transition-all active:scale-95 border-4 shadow-2xl",
                isEstopActive 
                  ? "bg-red-500 border-red-400 text-white animate-pulse" 
                  : "bg-zinc-950 border-zinc-800 text-red-500 hover:bg-red-500/5 hover:border-red-500/30"
              )}
            >
              <Power size={24} />
              <span className="text-[10px] font-black uppercase tracking-[0.3em]">
                {isEstopActive ? "E-Stop Active" : "Emergency Stop"}
              </span>
            </button>
          </section>

          {/* Vehicle Telemetry */}
          <section className="flex-1 overflow-hidden flex flex-col gap-3">
            <h2 className="text-[10px] font-mono text-zinc-500 uppercase tracking-widest flex items-center gap-2 px-2 flex-none">
              <Radio size={14} /> Vehicle Telemetry
            </h2>
            
            <div className="flex-1 overflow-y-auto space-y-3 pr-2 custom-scrollbar">
              {Object.entries(vehicles).map(([id, data]) => (
                <div key={id} className="bg-zinc-900/50 rounded-2xl border border-zinc-800 p-4 space-y-3">
                  <div className="flex justify-between items-center">
                    <div className="flex items-center gap-2">
                      <div className="w-6 h-6 bg-zinc-800 rounded flex items-center justify-center">
                        <Database size={12} className="text-orange-500" />
                      </div>
                      <div>
                        <div className="text-[8px] font-mono text-zinc-500 uppercase">Vehicle ID</div>
                        <div className="text-[10px] font-bold font-mono">V-00{id}</div>
                      </div>
                    </div>
                    <div className="text-right">
                      <div className="text-[8px] font-mono text-zinc-500 uppercase">Status</div>
                      <div className="text-[8px] font-mono text-green-500 uppercase">Active</div>
                    </div>
                  </div>

                  <div className="grid grid-cols-3 gap-2">
                    <div className="p-2 bg-zinc-950 rounded-xl border border-zinc-800">
                      <Gauge size={12} className="text-zinc-500 mb-1" />
                      <div className="text-[8px] font-mono text-zinc-500 uppercase">Speed</div>
                      <div className="text-xs font-bold font-mono">{data.speed?.toFixed(1)}</div>
                    </div>
                    <div className="p-2 bg-zinc-950 rounded-xl border border-zinc-800">
                      <Cpu size={12} className="text-zinc-500 mb-1" />
                      <div className="text-[8px] font-mono text-zinc-500 uppercase">Yaw</div>
                      <div className="text-xs font-bold font-mono">{data.yaw}°</div>
                    </div>
                    <div className="p-2 bg-zinc-950 rounded-xl border border-zinc-800">
                      <Thermometer size={12} className="text-zinc-500 mb-1" />
                      <div className="text-[8px] font-mono text-zinc-500 uppercase">Pitch</div>
                      <div className="text-xs font-bold font-mono">{data.pitch}°</div>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </section>
        </div>

        {/* Right Column: Visualization & Feed */}
        <div className="col-span-12 lg:col-span-8 flex flex-col gap-4 overflow-hidden">
          
          <div className="flex-1 grid grid-rows-2 gap-4 overflow-hidden">
            {/* Live Feed */}
            <div className="min-h-0">
              <VideoFeed />
            </div>

            {/* Track Visualization */}
            <div className="min-h-0">
              <TrackMap sensors={sensors} vehicles={vehicles} />
            </div>
          </div>

          {/* Sensor Grid */}
          <section className="bg-zinc-900/50 rounded-2xl border border-zinc-800 p-4 flex-none">
            <h2 className="text-[10px] font-mono text-zinc-500 uppercase tracking-widest flex items-center gap-2 mb-4">
              <Zap size={14} /> Sensor Network Status
            </h2>
            <div className="grid grid-cols-4 sm:grid-cols-6 md:grid-cols-8 lg:grid-cols-11 gap-2">
              {Object.entries(sensors).map(([id, state]) => (
                <div 
                  key={id}
                  className={cn(
                    "p-2 rounded-lg border transition-all flex flex-col items-center gap-1",
                    state === 1 
                      ? "bg-yellow-400/10 border-yellow-400/50 text-yellow-400" 
                      : "bg-zinc-950 border-zinc-800 text-zinc-600"
                  )}
                >
                  <div className={cn(
                    "w-1.5 h-1.5 rounded-full",
                    state === 1 ? "bg-yellow-400 shadow-[0_0_10px_rgba(250,204,21,0.5)]" : "bg-zinc-800"
                  )} />
                  <span className="text-[7px] font-mono uppercase tracking-tighter truncate w-full text-center">{id}</span>
                </div>
              ))}
            </div>
          </section>
        </div>
      </main>

      {/* Footer Info */}
      <footer className="h-10 flex-none border-t border-zinc-800 flex justify-between items-center px-6 text-[9px] font-mono text-zinc-500 uppercase tracking-widest bg-zinc-950/50">
        <div className="flex items-center gap-4">
          <span>v2.4.0-STABLE</span>
          <div className="w-px h-3 bg-zinc-800" />
          <span>SYNC: {new Date().toLocaleTimeString()}</span>
        </div>
        <div className="flex items-center gap-2">
          <AlertTriangle size={10} className="text-orange-500" />
          <span>Caution: High Voltage Area</span>
        </div>
      </footer>

      {/* E-Stop Overlay */}
      <AnimatePresence>
        {isEstopActive && (
          <motion.div 
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            className="fixed inset-0 z-[100] bg-red-600/20 backdrop-blur-sm pointer-events-none flex items-center justify-center"
          >
            <div className="bg-red-600 text-white px-12 py-6 rounded-full shadow-[0_0_50px_rgba(220,38,38,0.5)] flex items-center gap-6 border-4 border-white/20">
              <AlertCircle size={48} className="animate-bounce" />
              <div className="text-center">
                <div className="text-4xl font-black uppercase tracking-tighter">EMERGENCY STOP</div>
                <div className="text-xs font-mono uppercase tracking-[0.4em] opacity-80">All Vehicle Power Cut</div>
              </div>
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
}
