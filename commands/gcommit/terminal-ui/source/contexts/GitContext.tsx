import React, { createContext, useContext, useState, useEffect, useCallback, useMemo, useRef, ReactNode } from 'react';
import simpleGit, { SimpleGit } from 'simple-git';

interface GitState {
  git: SimpleGit;
  originalBranch: string;
  stagingBranch: string | null;
  stagedDiff: string;
}

interface GitContextValue extends GitState {
  createStagingBranch: () => Promise<string>;
  deleteStagingBranch: () => Promise<void>;
  mergeStagingBranch: () => Promise<void>;
  applyPatch: (patchContent: string) => Promise<void>;
  stageAll: () => Promise<void>;
  commit: (message: string) => Promise<void>;
  cleanup: () => Promise<void>;
}

const GitContext = createContext<GitContextValue | null>(null);

interface GitProviderProps {
  children: ReactNode;
  dev?: boolean;
}

export function GitProvider({ children, dev = false }: GitProviderProps) {
  const [state, setState] = useState<GitState>({
    // @ts-ignore
    git: simpleGit(),
    originalBranch: '',
    stagingBranch: null,
    stagedDiff: '',
  });

  const stagingBranchRef = useRef<string | null>(null);
  const originalBranchRef = useRef<string>('');

  useEffect(() => {
    state.git.branch().then(b => {
      setState(s => ({ ...s, originalBranch: b.current }));
      originalBranchRef.current = b.current;
    });
  }, []);

  useEffect(() => {
    stagingBranchRef.current = state.stagingBranch;
    originalBranchRef.current = state.originalBranch;
  }, [state.stagingBranch, state.originalBranch]);

  const createStagingBranch = useCallback(async (): Promise<string> => {
    const status = await state.git.status();
    const hasChanges = status.modified.length > 0 ||
                       status.not_added.length > 0 ||
                       status.deleted.length > 0 ||
                       status.staged.length > 0 ||
                       status.renamed.length > 0 ||
                       status.created.length > 0;

    if (hasChanges) {
      await state.git.stash(['push', '-u', '-m', 'gcommit-temp']);
    }

    const branchName = `gcommit/staging-${Date.now()}`;
    await state.git.checkoutLocalBranch(branchName);

    if (hasChanges) {
      await state.git.stash(['apply', '--index']);
    }

    const diff = await state.git.diff(['--cached']);
    setState(s => ({ ...s, stagedDiff: diff, stagingBranch: branchName }));

    await state.git.reset(['--hard']);
    return branchName;
  }, [state.git]);

  const deleteStagingBranch = useCallback(async () => {
    if (state.stagingBranch) {
      await state.git.checkout(state.originalBranch);
      await state.git.deleteLocalBranch(state.stagingBranch, true);
      setState(s => ({ ...s, stagingBranch: null }));
    }
  }, [state.git, state.stagingBranch, state.originalBranch]);

  const mergeStagingBranch = useCallback(async () => {
    if (state.stagingBranch) {
      await state.git.checkout(state.originalBranch);
      await state.git.merge([state.stagingBranch]);
      await state.git.deleteLocalBranch(state.stagingBranch, true);
      setState(s => ({ ...s, stagingBranch: null }));
    }
  }, [state.git, state.stagingBranch, state.originalBranch]);

  const applyPatch = useCallback(async (patchPath: string) => {
    try {
      await state.git.raw(['apply', '--unidiff-zero', patchPath]);
    } catch (err: any) {
      if (dev) {
        console.error(`Failed to apply patch ${patchPath}:`, err);
      }
      throw new Error(`Failed to apply patch ${patchPath}: ${err.message}`);
    }
  }, [state.git, dev]);

  const stageAll = useCallback(async () => {
    await state.git.add('-A');
  }, [state.git]);

  const commit = useCallback(async (message: string) => {
    await state.git.commit(message);
  }, [state.git]);

  const cleanup = useCallback(async () => {
    const stagingBranch = stagingBranchRef.current;
    const originalBranch = originalBranchRef.current;

    if (stagingBranch) {
      try {
        await state.git.add('-A');
        await state.git.commit("commit to cleanup");
      } catch {
        // No changes to commit before checkout
      }
      try {
        await state.git.checkout(originalBranch);
        await state.git.deleteLocalBranch(stagingBranch, true);
        await state.git.stash(['apply', "--index"]);
      } catch (err) {
        console.error("Failed during git cleanup:", err);
      }
    } else {
      console.error("No staging branch name found");
    }
  }, [state.git]);

  const value: GitContextValue = useMemo(() => ({
    ...state,
    createStagingBranch,
    deleteStagingBranch,
    mergeStagingBranch,
    applyPatch,
    stageAll,
    commit,
    cleanup,
  }), [state, createStagingBranch, deleteStagingBranch, mergeStagingBranch, applyPatch, stageAll, commit, cleanup]);

  return (
    <GitContext.Provider value={value}>
      {children}
    </GitContext.Provider>
  );
}

export function useGit(): GitContextValue {
  const context = useContext(GitContext);
  if (!context) {
    throw new Error('useGit must be used within a GitProvider');
  }
  return context;
}
