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
  // Environmental Zones
  const zones = [
    { id: 'Station', x: 350, y: 850, w: 150, h: 50, color: 'bg-orange-500/20', border: 'border-orange-500/40' },
    { id: 'Football', x: 20, y: 300, w: 130, h: 400, color: 'bg-green-500/10', border: 'border-green-500/30' },
    { id: 'Basketball', x: 350, y: 400, w: 250, h: 200, color: 'bg-orange-600/10', border: 'border-orange-600/30' },
    { id: 'Baseball', x: 700, y: 100, w: 250, h: 200, color: 'bg-red-500/10', border: 'border-red-500/30' },
  ];

  // Mathematically corrected SVG path to match the visual blueprint
  const trackPath = [
    "M 350 875",                 // Leave left side of Station
    "L 150 875",                 // Go West
    "Q 80 875 80 800",           // Curve North
    "L 80 300",                  // Up straight through Football
    "Q 80 100 200 100",          // Curve East at top left
    "C 350 100 300 400 475 400", // Dip down to top of Basketball
    "C 600 400 600 200 700 200", // Climb up to Baseball
    "L 880 200",                 // East into Baseball
    "C 950 200 950 260 880 260", // Hairpin turn inside Baseball
    "Q 800 260 800 350",         // Curve South out of Baseball
    "L 850 650",                 // Down towards Loop entry
    "C 850 800 950 850 950 750", // Loop outer right curve
    "C 950 650 750 650 750 750", // Loop top cross-over curve
    "C 750 850 800 920 800 920", // Loop bottom exit
    "L 550 920",                 // Return West towards Station
    "Q 500 920 500 875",         // Curve up into right side of Station
    "Z"                          // Close path loop
  ].join(" ");

  // Updated sensor positions to sit exactly on the new SVG path
  const sensorPositions: Record<string, { x: number; y: number }> = {
    Station1: { x: 380, y: 875 },
    Station2: { x: 470, y: 875 },
    Drop1: { x: 80, y: 500 },     // Inside Football
    Mid: { x: 260, y: 150 },      // The dip before Basketball
    Basket: { x: 475, y: 400 },   // Top edge of Basketball
    Rotate1: { x: 625, y: 280 },  // Climb towards Baseball
    Switch1: { x: 860, y: 200 },  // Entering Baseball hairpin
    Switch2: { x: 860, y: 260 },  // Exiting Baseball hairpin
    Drop2: { x: 825, y: 500 },    // The descent to the loop
    Centry: { x: 860, y: 750 },   // Inside the loop
    Rotate2: { x: 710, y: 920 },  // Final straightaway return
  };

  return (
    <div className="relative w-full h-full bg-zinc-950 rounded-2xl border border-zinc-800 overflow-hidden">
      <div className="absolute top-6 left-6 z-10">
        <h3 className="text-[10px] font-mono text-zinc-500 uppercase tracking-[0.3em]">Track Layout Blueprint</h3>
        <div className="text-[8px] font-mono text-zinc-600 mt-1">GRID: 1000x1000 | SCALE: 1:1</div>
      </div>

      {/* Zones Overlay */}
      <div className="absolute inset-0 pointer-events-none">
        {zones.map(zone => (
          <div
            key={zone.id}
            className={cn("absolute border border-dashed transition-colors duration-500", zone.color, zone.border)}
            style={{
              left: `${(zone.x / 1000) * 100}%`,
              top: `${(zone.y / 1000) * 100}%`,
              width: `${(zone.w / 1000) * 100}%`,
              height: `${(zone.h / 1000) * 100}%`,
            }}
          >
            <span className="absolute top-2 left-2 text-[8px] font-mono text-zinc-500 uppercase tracking-widest">
              {zone.id}
            </span>
          </div>
        ))}
      </div>
      
      <svg viewBox="0 0 1000 1000" className="w-full h-full relative z-0">
        {/* Grid Lines */}
        <defs>
          <pattern id="grid" width="100" height="100" patternUnits="userSpaceOnUse">
            <path d="M 100 0 L 0 0 0 100" fill="none" stroke="rgba(255,255,255,0.03)" strokeWidth="1"/>
          </pattern>
        </defs>
        <rect width="1000" height="1000" fill="url(#grid)" />

        {/* Thick Background Track */}
        <path
          d={trackPath}
          fill="none"
          stroke="#18181b"
          strokeWidth="30"
          strokeLinecap="round"
          strokeLinejoin="round"
        />
        {/* Dashed Center Line */}
        <path
          d={trackPath}
          fill="none"
          stroke="#27272a"
          strokeWidth="4"
          strokeDasharray="10 15"
        />

        {/* Sensors */}
        {Object.entries(sensorPositions).map(([id, pos]) => (
          <g key={id}>
            <circle
              cx={pos.x}
              cy={pos.y}
              r="10"
              className={cn(
                "transition-all duration-300",
                sensors[id] === 1 ? "fill-yellow-400 shadow-[0_0_20px_rgba(250,204,21,0.8)]" : "fill-zinc-800"
              )}
            />
            {sensors[id] === 1 && (
              <circle
                cx={pos.x}
                cy={pos.y}
                r="18"
                className="fill-yellow-400/20 animate-ping"
              />
            )}
            <text
              x={pos.x}
              y={pos.y - 20}
              textAnchor="middle"
              className="text-[12px] fill-zinc-500 font-mono font-bold uppercase"
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
              <rect
                x="-15"
                y="-25"
                width="30"
                height="50"
                rx="8"
                className="fill-orange-500 stroke-white stroke-[3] shadow-2xl"
              />
              {/* Headlights */}
              <circle cx="-8" cy="-20" r="3" className="fill-white" />
              <circle cx="8" cy="-20" r="3" className="fill-white" />
              
              <text
                dy="5"
                textAnchor="middle"
                className="text-[14px] fill-white font-black font-mono"
              >
                {id}
              </text>
              
              {/* Speed Vector (only shows if speed > 0) */}
              {data.speed > 0 && (
                <motion.path
                  d="M 0,-35 L 0,-60"
                  stroke="white"
                  strokeWidth="2"
                  strokeDasharray="4 4"
                  animate={{ strokeDashoffset: [0, -20] }}
                  transition={{ repeat: Infinity, duration: 0.5, ease: "linear" }}
                />
              )}
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