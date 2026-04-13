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
  // Grid Size: 823x453
  // Track Path Geometry rebuilt to match the sketch flows
  const trackPath = [
    "M 260,405",                  // Enter Station
    "L 420,405",                  // Leave Station
    "L 620,405",                  // Straight towards Loop
    "C 750,405 750,280 670,280",  // Loop top half
    "C 600,280 600,390 690,390",  // Loop bottom half (Centry)
    "C 750,390 750,200 700,140",  // Up towards Baseball
    "C 680,100 740,80 760,100",   // Curve right into Baseball
    "C 780,120 740,140 680,140",  // Hairpin left out of Baseball
    "L 480,140",                  // Straight left to Basketball
    "C 420,140 420,240 380,240",  // Curve down into Basketball
    "L 300,240",                  // Through Basketball
    "C 200,240 200,50 120,50",    // Left out and up over Football
    "C 60,50 60,100 60,150",      // Down into Football
    "L 60,320",                   // Straight down Football
    "C 60,405 180,405 260,405",   // Curve back to Station
    "Z"
  ].join(" ");

  // Sensor positions perfectly aligned onto the new SVG path
  const sensorPositions: Record<string, { x: number; y: number }> = {
    Station1: { x: 290, y: 405 },
    Station2: { x: 390, y: 405 },
    Centry: { x: 690, y: 390 },
    Switch1: { x: 760, y: 100 },
    Switch2: { x: 680, y: 140 },
    Rotate1: { x: 480, y: 140 },
    Rotate2: { x: 430, y: 190 },
    Basket: { x: 340, y: 240 },
    Mid: { x: 120, y: 50 },
    Drop1: { x: 60, y: 150 },
    Drop2: { x: 60, y: 300 },
  };

  return (
    <div className="relative w-full h-full bg-zinc-950 rounded-2xl border border-zinc-800 overflow-hidden">
      <div className="absolute top-6 left-6 z-10">
        <h3 className="text-[10px] font-mono text-zinc-500 uppercase tracking-[0.3em]">Track Layout Blueprint</h3>
        <div className="text-[8px] font-mono text-zinc-600 mt-1">GRID: 823x453 | SCALE: 1:1</div>
      </div>
      
      <svg viewBox="0 0 823 453" className="w-full h-full relative z-0">
        {/* Grid Lines */}
        <defs>
          <pattern id="grid" width="50" height="50" patternUnits="userSpaceOnUse">
            <path d="M 50 0 L 0 0 0 50" fill="none" stroke="rgba(255,255,255,0.03)" strokeWidth="1"/>
          </pattern>
        </defs>
        <rect width="823" height="453" fill="url(#grid)" />

        {/* Track Path (Slightly bumped opacity so you can see it clearer) */}
        <path
          d={trackPath}
          fill="none"
          stroke="#18181b"
          strokeWidth="15"
          strokeLinecap="round"
          strokeLinejoin="round"
          className="opacity-40"
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