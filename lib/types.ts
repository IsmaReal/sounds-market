export type Genre =
  | "Hip-Hop"
  | "Trap"
  | "Lo-Fi"
  | "House"
  | "Drum & Bass"
  | "Ambient"
  | "Pop"
  | "R&B"
  | "Techno"
  | "Soul";

export type MusicalKey =
  | "C"
  | "C#"
  | "D"
  | "D#"
  | "E"
  | "F"
  | "F#"
  | "G"
  | "G#"
  | "A"
  | "A#"
  | "B"
  | "Cm"
  | "C#m"
  | "Dm"
  | "D#m"
  | "Em"
  | "Fm"
  | "F#m"
  | "Gm"
  | "G#m"
  | "Am"
  | "A#m"
  | "Bm";

export interface Sample {
  id: string;
  title: string;
  author: string;
  authorAvatar: string;
  bpm: number;
  key: MusicalKey;
  genre: Genre;
  duration: number; // seconds
  waveform: number[]; // normalized 0-1 values for waveform display
  audioUrl: string; // mock URL
  tags: string[];
  downloads: number;
  plays: number;
  createdAt: string;
}
