"use client";

import Link from "next/link";
import { Download, Headphones, Music } from "lucide-react";
import { AudioPlayer } from "./audio-player";
import { Waveform } from "./waveform";
import { formatCount } from "@/lib/utils";
import type { Sample } from "@/lib/types";
interface SampleCardProps {
  sample: Sample;
}

export function SampleCard({ sample }: SampleCardProps) {
  return (
    <div
      className="group relative rounded-xl border border-border bg-card p-4 hover:border-primary/40 hover:bg-card/80 transition-all duration-200"
    >
      {/* Header row */}
      <div className="flex items-start gap-3 mb-3">
        {/* Play button */}
        <AudioPlayer sample={sample} compact />

        {/* Title + author */}
        <div className="flex-1 min-w-0">
          <Link
            href={`/sample/${sample.id}`}
            className="block font-semibold text-foreground truncate hover:text-primary transition-colors"
          >
            {sample.title}
          </Link>
          <div className="flex items-center gap-1 mt-0.5">
            <div className="flex h-5 w-5 items-center justify-center rounded-full bg-secondary text-xs font-bold text-muted-foreground shrink-0">
              {sample.authorAvatar}
            </div>
            <span className="text-xs text-muted-foreground truncate">
              {sample.author}
            </span>
          </div>
        </div>

        {/* Download button */}
        <Link
          href={`/sample/${sample.id}`}
          className="flex h-8 w-8 items-center justify-center rounded-lg border border-border text-muted-foreground hover:border-primary hover:text-primary transition-colors shrink-0"
          title="View & download"
        >
          <Download className="h-3.5 w-3.5" />
        </Link>
      </div>

      {/* Mini waveform (static preview) */}
      <div className="mb-3 pointer-events-none opacity-70">
        <Waveform
          data={sample.waveform.slice(0, 40)}
          progress={0}
          isPlaying={false}
        />
      </div>

      {/* Tags row */}
      <div className="flex items-center gap-2 flex-wrap">
        <Badge variant="genre">{sample.genre}</Badge>
        <Badge variant="stat">
          <Music className="h-2.5 w-2.5" />
          {sample.bpm} BPM
        </Badge>
        <Badge variant="stat">{sample.key}</Badge>
        <div className="ml-auto flex items-center gap-3 text-xs text-muted-foreground">
          <span className="flex items-center gap-1">
            <Headphones className="h-3 w-3" />
            {formatCount(sample.plays)}
          </span>
          <span className="flex items-center gap-1">
            <Download className="h-3 w-3" />
            {formatCount(sample.downloads)}
          </span>
        </div>
      </div>
    </div>
  );
}

function Badge({
  children,
  variant,
}: {
  children: React.ReactNode;
  variant: "genre" | "stat";
}) {
  const base = "inline-flex items-center gap-1 rounded-md px-2 py-0.5 text-xs font-medium";
  const styles =
    variant === "genre"
      ? "bg-primary/15 text-primary"
      : "bg-secondary text-muted-foreground";
  return <span className={`${base} ${styles}`}>{children}</span>;
}
