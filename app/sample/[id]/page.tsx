import { notFound } from "next/navigation";
import Link from "next/link";
import {
  ArrowLeft,
  Download,
  Music,
  Headphones,
  Clock,
  Tag,
  Calendar,
} from "lucide-react";
import { SAMPLES } from "@/lib/mock-data";
import { AudioPlayer } from "@/components/audio-player";
import { formatCount, formatDuration } from "@/lib/utils";
import { SampleCard } from "@/components/sample-card";

interface PageProps {
  params: Promise<{ id: string }>;
}

export function generateStaticParams() {
  return SAMPLES.map((s) => ({ id: s.id }));
}

export default async function SamplePage({ params }: PageProps) {
  const { id } = await params;
  const sample = SAMPLES.find((s) => s.id === id);
  if (!sample) notFound();

  const related = SAMPLES.filter(
    (s) => s.id !== sample.id && (s.genre === sample.genre || s.key === sample.key)
  ).slice(0, 3);

  return (
    <div className="mx-auto max-w-3xl px-4 py-8 sm:px-6">
      {/* Back */}
      <Link
        href="/"
        className="mb-6 inline-flex items-center gap-1.5 text-sm text-muted-foreground hover:text-foreground transition-colors"
      >
        <ArrowLeft className="h-4 w-4" />
        Back to feed
      </Link>

      {/* Main card */}
      <div className="rounded-2xl border border-border bg-card p-6">
        {/* Title */}
        <div className="flex items-start justify-between gap-4 mb-6">
          <div>
            <h1 className="text-2xl font-bold text-foreground">{sample.title}</h1>
            <div className="mt-1 flex items-center gap-2">
              <div className="flex h-6 w-6 items-center justify-center rounded-full bg-secondary text-xs font-bold text-muted-foreground">
                {sample.authorAvatar}
              </div>
              <span className="text-sm text-muted-foreground">{sample.author}</span>
            </div>
          </div>
          {/* Genre badge */}
          <span className="shrink-0 rounded-lg bg-primary/15 px-3 py-1 text-sm font-medium text-primary">
            {sample.genre}
          </span>
        </div>

        {/* Audio player */}
        <AudioPlayer sample={sample} />

        {/* Stats grid */}
        <div className="mt-6 grid grid-cols-2 gap-3 sm:grid-cols-4">
          <StatCard icon={<Music className="h-4 w-4" />} label="BPM" value={String(sample.bpm)} />
          <StatCard icon={<Music className="h-4 w-4" />} label="Key" value={sample.key} />
          <StatCard icon={<Clock className="h-4 w-4" />} label="Duration" value={formatDuration(sample.duration)} />
          <StatCard icon={<Headphones className="h-4 w-4" />} label="Plays" value={formatCount(sample.plays)} />
        </div>

        {/* Tags */}
        <div className="mt-5">
          <div className="flex items-center gap-2 flex-wrap">
            <Tag className="h-3.5 w-3.5 text-muted-foreground" />
            {sample.tags.map((tag) => (
              <span
                key={tag}
                className="rounded-md bg-secondary px-2 py-0.5 text-xs text-muted-foreground"
              >
                #{tag}
              </span>
            ))}
          </div>
        </div>

        {/* Meta */}
        <div className="mt-4 flex items-center gap-1 text-xs text-muted-foreground">
          <Calendar className="h-3 w-3" />
          <span>Added {new Date(sample.createdAt).toLocaleDateString("en-US", { year: "numeric", month: "long", day: "numeric" })}</span>
          <span className="mx-2">·</span>
          <Download className="h-3 w-3" />
          <span>{formatCount(sample.downloads)} downloads</span>
        </div>

        {/* Download CTA */}
        <div className="mt-6 pt-6 border-t border-border">
          <button className="w-full flex items-center justify-center gap-2 rounded-xl bg-primary py-3 text-sm font-semibold text-primary-foreground hover:opacity-90 active:scale-[0.98] transition-all">
            <Download className="h-4 w-4" />
            Download Sample (Free)
          </button>
          <p className="mt-2 text-center text-xs text-muted-foreground">
            Royalty-free · Use in any project
          </p>
        </div>
      </div>

      {/* Related samples */}
      {related.length > 0 && (
        <div className="mt-10">
          <h2 className="mb-4 text-lg font-semibold">Related Samples</h2>
          <div className="flex flex-col gap-3">
            {related.map((s) => (
              <SampleCard key={s.id} sample={s} />
            ))}
          </div>
        </div>
      )}
    </div>
  );
}

function StatCard({
  icon,
  label,
  value,
}: {
  icon: React.ReactNode;
  label: string;
  value: string;
}) {
  return (
    <div className="flex flex-col gap-1 rounded-lg bg-secondary/60 px-3 py-2.5">
      <div className="flex items-center gap-1.5 text-xs text-muted-foreground">
        {icon}
        {label}
      </div>
      <span className="text-base font-semibold text-foreground">{value}</span>
    </div>
  );
}
