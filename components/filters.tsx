"use client";

import { X } from "lucide-react";
import { GENRES, BPM_RANGES, KEYS } from "@/lib/mock-data";

export interface FilterState {
  genre: string | null;
  bpmRange: { min: number; max: number } | null;
  key: string | null;
  query: string;
}

interface FiltersProps {
  filters: FilterState;
  onChange: (filters: FilterState) => void;
}

export function Filters({ filters, onChange }: FiltersProps) {
  const hasActive = filters.genre || filters.bpmRange || filters.key;

  function clearAll() {
    onChange({ genre: null, bpmRange: null, key: null, query: filters.query });
  }

  return (
    <div className="flex flex-wrap items-center gap-2">
      {/* Genre */}
      <FilterGroup label="Genre">
        {GENRES.map((g) => (
          <Chip
            key={g}
            active={filters.genre === g}
            onClick={() =>
              onChange({ ...filters, genre: filters.genre === g ? null : g })
            }
          >
            {g}
          </Chip>
        ))}
      </FilterGroup>

      {/* BPM */}
      <FilterGroup label="BPM">
        {BPM_RANGES.map((r) => (
          <Chip
            key={r.label}
            active={filters.bpmRange?.min === r.min}
            onClick={() =>
              onChange({
                ...filters,
                bpmRange:
                  filters.bpmRange?.min === r.min
                    ? null
                    : { min: r.min, max: r.max },
              })
            }
          >
            {r.label}
          </Chip>
        ))}
      </FilterGroup>

      {/* Key */}
      <FilterGroup label="Key">
        <div className="flex flex-wrap gap-1">
          {KEYS.map((k) => (
            <Chip
              key={k}
              active={filters.key === k}
              onClick={() =>
                onChange({ ...filters, key: filters.key === k ? null : k })
              }
            >
              {k}
            </Chip>
          ))}
        </div>
      </FilterGroup>

      {/* Clear */}
      {hasActive && (
        <button
          onClick={clearAll}
          className="flex items-center gap-1 rounded-md px-2 py-1 text-xs text-muted-foreground hover:text-foreground hover:bg-secondary transition-colors"
        >
          <X className="h-3 w-3" />
          Clear
        </button>
      )}
    </div>
  );
}

function FilterGroup({
  label,
  children,
}: {
  label: string;
  children: React.ReactNode;
}) {
  return (
    <div className="flex items-center gap-2 flex-wrap">
      <span className="text-xs text-muted-foreground font-medium shrink-0">
        {label}:
      </span>
      <div className="flex flex-wrap gap-1">{children}</div>
    </div>
  );
}

function Chip({
  children,
  active,
  onClick,
}: {
  children: React.ReactNode;
  active: boolean;
  onClick: () => void;
}) {
  return (
    <button
      onClick={onClick}
      className={`rounded-md px-2.5 py-1 text-xs font-medium transition-colors ${
        active
          ? "bg-primary text-primary-foreground"
          : "bg-secondary text-muted-foreground hover:text-foreground hover:bg-secondary/80"
      }`}
    >
      {children}
    </button>
  );
}
