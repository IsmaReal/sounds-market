"use client";

import { useEffect, useRef, useState } from "react";
import { Pause, Play, RotateCcw } from "lucide-react";
import { Waveform } from "./waveform";
import { formatDuration } from "@/lib/utils";
import type { Sample } from "@/lib/types";

interface AudioPlayerProps {
  sample: Sample;
  compact?: boolean;
}

export function AudioPlayer({ sample, compact = false }: AudioPlayerProps) {
  const audioRef = useRef<HTMLAudioElement | null>(null);
  const [isPlaying, setIsPlaying] = useState(false);
  const [progress, setProgress] = useState(0); // 0-1
  const [currentTime, setCurrentTime] = useState(0);

  // In MVP there's no real audio file — we simulate playback with a timer
  const timerRef = useRef<ReturnType<typeof setInterval> | null>(null);

  function startSimulation() {
    timerRef.current = setInterval(() => {
      setCurrentTime((prev) => {
        const next = prev + 0.1;
        if (next >= sample.duration) {
          clearInterval(timerRef.current!);
          setIsPlaying(false);
          setProgress(0);
          return 0;
        }
        setProgress(next / sample.duration);
        return next;
      });
    }, 100);
  }

  function stopSimulation() {
    if (timerRef.current) {
      clearInterval(timerRef.current);
      timerRef.current = null;
    }
  }

  function togglePlay() {
    if (isPlaying) {
      stopSimulation();
      setIsPlaying(false);
    } else {
      setIsPlaying(true);
      startSimulation();
    }
  }

  function handleSeek(p: number) {
    stopSimulation();
    setProgress(p);
    setCurrentTime(p * sample.duration);
    if (isPlaying) {
      startSimulation();
    }
  }

  function reset() {
    stopSimulation();
    setIsPlaying(false);
    setProgress(0);
    setCurrentTime(0);
  }

  useEffect(() => {
    return () => stopSimulation();
  }, []);

  if (compact) {
    return (
      <button
        onClick={(e) => {
          e.preventDefault();
          e.stopPropagation();
          togglePlay();
        }}
        className="flex h-9 w-9 items-center justify-center rounded-full bg-primary text-primary-foreground hover:scale-105 active:scale-95 transition-transform shrink-0"
        aria-label={isPlaying ? "Pause" : "Play"}
      >
        {isPlaying ? (
          <Pause className="h-4 w-4 fill-current" />
        ) : (
          <Play className="h-4 w-4 fill-current translate-x-0.5" />
        )}
      </button>
    );
  }

  return (
    <div className="flex flex-col gap-3">
      <Waveform
        data={sample.waveform}
        progress={progress}
        isPlaying={isPlaying}
        onSeek={handleSeek}
      />
      <div className="flex items-center gap-3">
        <button
          onClick={togglePlay}
          className="flex h-10 w-10 items-center justify-center rounded-full bg-primary text-primary-foreground hover:scale-105 active:scale-95 transition-transform shrink-0"
        >
          {isPlaying ? (
            <Pause className="h-4 w-4 fill-current" />
          ) : (
            <Play className="h-4 w-4 fill-current translate-x-0.5" />
          )}
        </button>
        <div className="flex items-center gap-2 text-xs text-muted-foreground tabular-nums">
          <span>{formatDuration(Math.floor(currentTime))}</span>
          <span>/</span>
          <span>{formatDuration(sample.duration)}</span>
        </div>
        <button
          onClick={reset}
          className="ml-auto text-muted-foreground hover:text-foreground transition-colors"
          aria-label="Reset"
        >
          <RotateCcw className="h-4 w-4" />
        </button>
      </div>
    </div>
  );
}
