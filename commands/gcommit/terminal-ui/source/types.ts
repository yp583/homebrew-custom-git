export type Point = {
  id: number;
  x: number;
  y: number;
  cluster_id: number;
  filepath: string;
  preview: string;
};

export type Cluster = {
  id: number;
  message: string;
};

export type CommitData = {
  cluster_id: number;
  message: string;
  patch_files: string[];
};

export type ProcessingResult = {
  visualization: {
    points: Point[];
    clusters: Cluster[];
  };
  commits: CommitData[];
};

// Dendrogram types
export type MergeEvent = {
  left: number;
  right: number;
  distance: number;
};

export type DendrogramData = {
  labels: string[];      // filepath for each leaf (chunk)
  merges: MergeEvent[];  // merge events to draw the tree
  max_distance: number;  // for scaling x-axis
};

export type MergePhaseResult = {
  dendrogram: DendrogramData;
  chunks: unknown[];  // opaque - only used by C++ in phase 2
};

export type Phase =
  | 'init'
  | 'dev-confirm'
  | 'processing'
  | 'dendrogram'
  | 'threshold-processing'
  | 'applying'
  | 'visualization'
  | 'merging'
  | 'restoring'
  | 'done'
  | 'cancelled'
  | 'error';

export type DiffLine = {
  content: string;
  type: 'addition' | 'deletion' | 'context';
};
