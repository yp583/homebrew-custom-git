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
  const hasUnstagedStashRef = useRef<boolean>(false);
  const hasStagedStashRef = useRef<boolean>(false);
  const stagedDiffRef = useRef<string>('');

  const findStashByName = async (name: string): Promise<string | null> => {
    const result = await state.git.stash(['list']);
    const lines = result.split('\n');
    for (const line of lines) {
      if (line.includes(name)) {
        const match = line.match(/^(stash@\{\d+\})/);
        if (match && match[1]) return match[1];
      }
    }
    return null;
  };

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
    // Capture staged diff before anything else
    const diff = await state.git.diff(['--cached']);
    stagedDiffRef.current = diff;

    const status = await state.git.status();

    // First, stash unstaged + untracked (keep staged in place with -k)
    const hasUnstagedChanges = status.modified.length > 0 ||
                                status.not_added.length > 0 ||
                                status.deleted.length > 0;
    if (hasUnstagedChanges) {
      await state.git.stash(['push', '-u', '-k', '-m', 'gcommit-unstaged']);
      hasUnstagedStashRef.current = true;
    }

    // Now stash the staged changes separately (so we can restore on cancel)
    const hasStagedChanges = status.staged.length > 0;
    if (hasStagedChanges) {
      await state.git.stash(['push', '-m', 'gcommit-staged']);
      hasStagedStashRef.current = true;
    }

    // Create staging branch from clean state
    const branchName = `gcommit/staging-${Date.now()}`;
    await state.git.checkoutLocalBranch(branchName);

    setState(s => ({ ...s, stagedDiff: diff, stagingBranch: branchName }));
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

      // Staged stash - apply it (changes already committed, may conflict/no-op)
      if (hasStagedStashRef.current) {
        try {
          const stagedRef = await findStashByName('gcommit-staged');
          if (stagedRef) await state.git.stash(['apply', '--index', stagedRef]);
        } catch {
          // Expected - staged changes already committed
        }
        hasStagedStashRef.current = false;
      }

      // Restore unstaged changes
      if (hasUnstagedStashRef.current) {
        const unstagedRef = await findStashByName('gcommit-unstaged');
        if (unstagedRef) await state.git.stash(['apply', unstagedRef]);
        hasUnstagedStashRef.current = false;
      }

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
        await state.git.checkout(originalBranch);
        await state.git.deleteLocalBranch(stagingBranch, true);
      } catch (err) {
        console.error("Failed during git cleanup:", err);
      }
    }

    // Restore staged changes with --index to keep them staged
    if (hasStagedStashRef.current) {
      try {
        const stagedRef = await findStashByName('gcommit-staged');
        if (stagedRef) await state.git.stash(['apply', '--index', stagedRef]);
        hasStagedStashRef.current = false;
      } catch (err) {
        console.error("Failed to restore staged changes:", err);
      }
    }

    // Restore unstaged changes
    if (hasUnstagedStashRef.current) {
      try {
        const unstagedRef = await findStashByName('gcommit-unstaged');
        if (unstagedRef) await state.git.stash(['apply', unstagedRef]);
        hasUnstagedStashRef.current = false;
      } catch (err) {
        console.error("Failed to restore unstaged changes:", err);
      }
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
