import React, { useState } from 'react';
import { Camera, Settings, RefreshCw, Maximize2 } from 'lucide-react';

export const VideoFeed: React.FC = () => {
  const [url, setUrl] = useState<string>('http://192.168.1.100/stream'); // Default placeholder
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);
  const [isLoaded, setIsLoaded] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const handleRefresh = () => {
    setIsLoaded(false);
    setError(null);
    // Force reload by appending a timestamp
    const newUrl = url.split('?')[0] + '?t=' + Date.now();
    setUrl(newUrl);
  };

  return (
    <div className="relative w-full h-full bg-zinc-950 rounded-xl border border-zinc-800 overflow-hidden group">
      {/* Header Overlay */}
      <div className="absolute top-0 left-0 right-0 p-4 bg-gradient-to-b from-black/80 to-transparent z-10 flex justify-between items-center opacity-0 group-hover:opacity-100 transition-opacity">
        <div className="flex items-center gap-2">
          <div className="w-2 h-2 rounded-full bg-red-500 animate-pulse" />
          <span className="text-[10px] font-mono text-white uppercase tracking-widest">ESP32-CAM LIVE</span>
        </div>
        <div className="flex items-center gap-3">
          <button 
            onClick={handleRefresh}
            className="p-1.5 rounded-lg bg-white/10 hover:bg-white/20 text-white transition-colors"
          >
            <RefreshCw size={14} />
          </button>
          <button 
            onClick={() => setIsSettingsOpen(!isSettingsOpen)}
            className="p-1.5 rounded-lg bg-white/10 hover:bg-white/20 text-white transition-colors"
          >
            <Settings size={14} />
          </button>
          <button className="p-1.5 rounded-lg bg-white/10 hover:bg-white/20 text-white transition-colors">
            <Maximize2 size={14} />
          </button>
        </div>
      </div>

      {/* Settings Overlay */}
      {isSettingsOpen && (
        <div className="absolute inset-0 bg-black/90 z-20 flex items-center justify-center p-8">
          <div className="w-full max-w-sm space-y-4">
            <h3 className="text-white font-mono text-sm uppercase tracking-widest">Camera Config</h3>
            <div className="space-y-2">
              <label className="text-[10px] text-zinc-500 uppercase font-mono">Stream URL</label>
              <input 
                type="text" 
                value={url} 
                onChange={(e) => setUrl(e.target.value)}
                className="w-full bg-zinc-800 border border-zinc-700 rounded-lg px-4 py-2 text-white font-mono text-xs focus:outline-none focus:border-orange-500"
                placeholder="http://192.168.1.x/stream"
              />
            </div>
            <button 
              onClick={() => setIsSettingsOpen(false)}
              className="w-full bg-orange-500 hover:bg-orange-600 text-white font-mono text-xs uppercase py-2 rounded-lg transition-colors"
            >
              Apply & Close
            </button>
          </div>
        </div>
      )}

      {/* Video Content */}
      <div className="w-full h-full flex items-center justify-center">
        {!isLoaded && !error && (
          <div className="flex flex-col items-center gap-4">
            <div className="w-8 h-8 border-2 border-orange-500/20 border-t-orange-500 rounded-full animate-spin" />
            <span className="text-[10px] font-mono text-zinc-500 uppercase tracking-widest">Establishing Stream...</span>
          </div>
        )}
        
        {error && (
          <div className="flex flex-col items-center gap-4 text-red-500">
            <Camera size={32} strokeWidth={1.5} />
            <span className="text-[10px] font-mono uppercase tracking-widest">Stream Connection Failed</span>
          </div>
        )}

        <img 
          src={url} 
          alt="ESP32 Camera Feed"
          className={`w-full h-full object-cover transition-opacity duration-500 ${isLoaded ? 'opacity-100' : 'opacity-0'}`}
          onLoad={() => setIsLoaded(true)}
          onError={() => {
            setError('Failed to load stream');
            setIsLoaded(false);
          }}
          referrerPolicy="no-referrer"
        />
      </div>

    </div>
  );
};
