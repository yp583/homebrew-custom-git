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

export type Phase =
  | 'init'
  | 'dev-confirm'
  | 'processing'
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
