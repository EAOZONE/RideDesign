import React from 'react';
import { motion } from 'motion/react';
import { clsx, type ClassValue } from 'clsx';
import { twMerge } from 'tailwind-merge';

function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

interface TrackMapProps {
  sensors: Record<string, number>;
  vehicles: Record<string, any>;
}

export const TrackMap: React.FC<TrackMapProps> = ({ sensors, vehicles }) => {
  // Grid Size: 1000x1000
  // Environmental Zones adjusted for 823x453
  const zones = [
    { id: 'Station', x: 200, y: 420, w: 250, h: 33, color: 'bg-orange-500/20', border: 'border-orange-500/40' },
    { id: 'Football', x: 20, y: 30, w: 200, h: 380, color: 'bg-green-500/10', border: 'border-green-500/30' },
    { id: 'Basketball', x: 400, y: 140, w: 150, h: 100, color: 'bg-orange-600/10', border: 'border-orange-600/30' },
    { id: 'Baseball', x: 680, y: 40, w: 140, h: 120, color: 'bg-red-500/10', border: 'border-red-500/30' },
  ];

  // Track Path Geometry based on the simplified sequence:
  // Station -> Loop -> Switch -> Rotator -> Basketball -> Football -> Station
  const trackPath = [
    "M 240,445", // Station 1
    "L 380,445", // Station 2
    "L 550,445", // Straight East
    "C 750,445 850,400 800,300", // Loop up
    "C 750,200 650,250 700,445", // Loop back to Centry
    "L 780,100", // Switch 1
    "L 720,90",  // Switch 2
    "L 530,90",  // Rotate 1
    "L 500,110", // Rotate 2
    "Q 450,110 450,190", // Basket
    "L 180,50",  // Mid
    "L 70,140",  // Drop 1
    "L 65,370",  // Drop 2
    "L 65,445",  // 90 degree turn corner
    "L 240,445", // Back to Station 1
    "Z"
  ].join(" ");

  // Updated sensor positions based on the new dashboard.js
  const sensorPositions: Record<string, { x: number; y: number }> = {
    Station1: { x: 240, y: 445 },
    Station2: { x: 380, y: 445 },
    Centry: { x: 700, y: 445 },
    Switch1: { x: 780, y: 100 },
    Switch2: { x: 720, y: 90 },
    Rotate1: { x: 530, y: 90 },
    Rotate2: { x: 500, y: 110 },
    Basket: { x: 450, y: 190 },
    Mid: { x: 180, y: 50 },
    Drop1: { x: 70, y: 140 },
    Drop2: { x: 65, y: 370 },
  };

  return (
    <div className="relative w-full h-full bg-zinc-950 rounded-2xl border border-zinc-800 overflow-hidden">
      <div className="absolute top-6 left-6 z-10">
        <h3 className="text-[10px] font-mono text-zinc-500 uppercase tracking-[0.3em]">Track Layout Blueprint</h3>
        <div className="text-[8px] font-mono text-zinc-600 mt-1">GRID: 823x453 | SCALE: 1:1</div>
      </div>

      {/* Zones Overlay */}
      <div className="absolute inset-0 pointer-events-none">
        {zones.map(zone => (
          <div
            key={zone.id}
            className={cn("absolute border border-dashed transition-colors duration-500", zone.color, zone.border)}
            style={{
              left: `${(zone.x / 823) * 100}%`,
              top: `${(zone.y / 453) * 100}%`,
              width: `${(zone.w / 823) * 100}%`,
              height: `${(zone.h / 453) * 100}%`,
            }}
          >
            <span className="absolute top-2 left-2 text-[8px] font-mono text-zinc-500 uppercase tracking-widest">
              {zone.id}
            </span>
          </div>
        ))}
      </div>
      
      <svg viewBox="0 0 823 453" className="w-full h-full relative z-0">
        {/* Grid Lines */}
        <defs>
          <pattern id="grid" width="50" height="50" patternUnits="userSpaceOnUse">
            <path d="M 50 0 L 0 0 0 50" fill="none" stroke="rgba(255,255,255,0.03)" strokeWidth="1"/>
          </pattern>
        </defs>
        <rect width="823" height="453" fill="url(#grid)" />

        {/* Track Path (Subtle) */}
        <path
          d={trackPath}
          fill="none"
          stroke="#18181b"
          strokeWidth="20"
          strokeLinecap="round"
          strokeLinejoin="round"
          className="opacity-20"
        />

        {/* Sensors */}
        {Object.entries(sensorPositions).map(([id, pos]) => (
          <g key={id}>
            <circle
              cx={pos.x}
              cy={pos.y}
              r="6"
              className={cn(
                "transition-all duration-300",
                sensors[id] === 1 ? "fill-yellow-400 shadow-[0_0_20px_rgba(250,204,21,0.8)]" : "fill-zinc-800"
              )}
            />
            {sensors[id] === 1 && (
              <circle
                cx={pos.x}
                cy={pos.y}
                r="12"
                className="fill-yellow-400/20 animate-ping"
              />
            )}
            <text
              x={pos.x}
              y={pos.y - 12}
              textAnchor="middle"
              className="text-[10px] fill-zinc-500 font-mono font-bold uppercase"
            >
              {id}
            </text>
          </g>
        ))}

        {/* Vehicles */}
        {Object.entries(vehicles).map(([id, data]) => {
          // Find the most recently triggered sensor to position the vehicle
          const activeSensor = Object.keys(sensors).find(s => sensors[s] === 1);
          const pos = activeSensor ? sensorPositions[activeSensor] : sensorPositions.Station1;

          return (
            <motion.g
              key={id}
              initial={false}
              animate={{ x: pos.x, y: pos.y }}
              transition={{ type: "spring", stiffness: 40, damping: 15 }}
            >
              {/* Vehicle Body */}
              <circle
                r="8"
                className="fill-orange-500 stroke-white stroke-[2] shadow-2xl"
              />
              
              <text
                dy="4"
                textAnchor="middle"
                className="text-[10px] fill-white font-black font-mono"
              >
                {id}
              </text>
            </motion.g>
          );
        })}
      </svg>

      {/* Legend Overlay */}
      <div className="absolute bottom-6 left-6 flex flex-col gap-2">
        <div className="flex items-center gap-3">
          <div className="w-3 h-3 rounded-full bg-orange-500 border-2 border-white" />
          <span className="text-[10px] font-mono text-zinc-400 uppercase tracking-widest">Vehicle V-00X</span>
        </div>
        <div className="flex items-center gap-3">
          <div className="w-3 h-3 rounded-full bg-yellow-400" />
          <span className="text-[10px] font-mono text-zinc-400 uppercase tracking-widest">Active Sensor</span>
        </div>
      </div>
    </div>
  );
};
