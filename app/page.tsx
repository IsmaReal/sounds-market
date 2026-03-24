"use client";

import { useSearchParams } from "next/navigation";
import { useEffect, useMemo, useRef, useState, Suspense } from "react";
import { SampleCard } from "@/components/sample-card";
import { Filters, type FilterState } from "@/components/filters";
import { SAMPLES } from "@/lib/mock-data";

const PAGE_SIZE = 6;

function FeedContent() {
  const searchParams = useSearchParams();
  const urlQuery = searchParams.get("q") ?? "";

  const [filters, setFilters] = useState<FilterState>({
    genre: null,
    bpmRange: null,
    key: null,
    query: urlQuery,
  });
  const [page, setPage] = useState(1);
  const loaderRef = useRef<HTMLDivElement | null>(null);

  // Sync URL query into filters
  useEffect(() => {
    setFilters((f) => ({ ...f, query: urlQuery }));
    setPage(1);
  }, [urlQuery]);

  const filtered = useMemo(() => {
    return SAMPLES.filter((s) => {
      if (filters.genre && s.genre !== filters.genre) return false;
      if (filters.key && s.key !== filters.key) return false;
      if (filters.bpmRange) {
        if (s.bpm < filters.bpmRange.min || s.bpm > filters.bpmRange.max)
          return false;
      }
      if (filters.query) {
        const q = filters.query.toLowerCase();
        if (
          !s.title.toLowerCase().includes(q) &&
          !s.author.toLowerCase().includes(q) &&
          !s.genre.toLowerCase().includes(q) &&
          !s.tags.some((t) => t.toLowerCase().includes(q))
        )
          return false;
      }
      return true;
    });
  }, [filters]);

  const visible = filtered.slice(0, page * PAGE_SIZE);
  const hasMore = visible.length < filtered.length;

  // Infinite scroll via IntersectionObserver
  useEffect(() => {
    if (!loaderRef.current || !hasMore) return;
    const observer = new IntersectionObserver(
      (entries) => {
        if (entries[0].isIntersecting) setPage((p) => p + 1);
      },
      { threshold: 0.1 }
    );
    observer.observe(loaderRef.current);
    return () => observer.disconnect();
  }, [hasMore]);

  return (
    <div className="mx-auto max-w-7xl px-4 py-8 sm:px-6">
      {/* Hero */}
      <div className="mb-8 text-center">
        <h1 className="text-3xl font-bold tracking-tight sm:text-4xl">
          Discover{" "}
          <span className="text-primary">Samples & Loops</span>
        </h1>
        <p className="mt-2 text-muted-foreground">
          Browse thousands of royalty-free sounds. Click to preview.
        </p>
      </div>

      {/* Filters */}
      <div className="mb-6 rounded-xl border border-border bg-card p-4">
        <Filters
          filters={filters}
          onChange={(f) => {
            setFilters(f);
            setPage(1);
          }}
        />
      </div>

      {/* Results count */}
      <div className="mb-4 flex items-center justify-between">
        <p className="text-sm text-muted-foreground">
          {filtered.length} sample{filtered.length !== 1 ? "s" : ""} found
        </p>
      </div>

      {/* Grid */}
      {filtered.length === 0 ? (
        <div className="flex flex-col items-center justify-center py-20 text-center">
          <p className="text-lg font-medium text-muted-foreground">
            No samples match your filters
          </p>
          <button
            onClick={() =>
              setFilters({ genre: null, bpmRange: null, key: null, query: "" })
            }
            className="mt-3 text-sm text-primary hover:underline"
          >
            Clear all filters
          </button>
        </div>
      ) : (
        <div className="grid grid-cols-1 gap-3 sm:grid-cols-2 lg:grid-cols-3">
          {visible.map((sample) => (
            <SampleCard key={sample.id} sample={sample} />
          ))}
        </div>
      )}

      {/* Infinite scroll trigger */}
      {hasMore && (
        <div ref={loaderRef} className="mt-8 flex justify-center">
          <div className="flex gap-1">
            {[0, 1, 2].map((i) => (
              <div
                key={i}
                className="h-2 w-2 rounded-full bg-primary/40 animate-bounce"
                style={{ animationDelay: `${i * 0.15}s` }}
              />
            ))}
          </div>
        </div>
      )}
    </div>
  );
}

export default function HomePage() {
  return (
    <Suspense>
      <FeedContent />
    </Suspense>
  );
}
