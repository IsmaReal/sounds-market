"use client";

interface WaveformProps {
  data: number[];
  progress: number; // 0-1
  isPlaying: boolean;
  onSeek?: (progress: number) => void;
  className?: string;
}

export function Waveform({
  data,
  progress,
  isPlaying: _isPlaying,
  onSeek,
  className = "",
}: WaveformProps) {
  const bars = data.length;

  function handleClick(e: React.MouseEvent<HTMLDivElement>) {
    if (!onSeek) return;
    const rect = e.currentTarget.getBoundingClientRect();
    const x = e.clientX - rect.left;
    onSeek(Math.max(0, Math.min(1, x / rect.width)));
  }

  return (
    <div
      className={`flex items-center gap-px h-10 cursor-pointer ${className}`}
      onClick={handleClick}
    >
      {data.map((value, i) => {
        const filled = i / bars <= progress;
        return (
          <div
            key={i}
            className="flex-1 rounded-sm transition-colors"
            style={{
              height: `${Math.max(10, value * 100)}%`,
              backgroundColor: filled
                ? "hsl(263 85% 65%)"
                : "hsl(224 15% 28%)",
              animationDelay: `${(i % 5) * 0.12}s`,
            }}
          />
        );
      })}
    </div>
  );
}
