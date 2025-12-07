import React, { useState, useEffect, useCallback } from 'react';
import { Box, Text, useApp, useInput } from 'ink';
import { Spinner } from '@inkjs/ui';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { GitProvider, useGit } from './contexts/GitContext.js';
import FileTree from './components/FileTree.js';
import DiffViewer from './components/DiffViewer.js';
import ScatterPlot from './components/ScatterPlot.js';
import ClusterLegend from './components/ClusterLegend.js';
import Dendrogram from './components/Dendrogram.js';
import type { Phase, ProcessingResult, DiffLine, MergePhaseResult, DendrogramData } from './types.js';
import { parseFullContextDiff } from './utils/diffUtils.js';

type Props = {
  threshold: number;
  verbose: boolean;
  dev: boolean;
};

function AppContent({ threshold, verbose, dev }: Props) {
  const { exit } = useApp();
  const git = useGit();

  const [phase, setPhase] = useState<Phase>('init');
  const [pendingPhase, setPendingPhase] = useState<Phase | null>(null);
  const [processingResult, setProcessingResult] = useState<ProcessingResult | null>(null);
  const [commitMessages, setCommitMessages] = useState<string[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [statusMessage, setStatusMessage] = useState('Initializing...');
  const [stderr, setStderr] = useState<string>('');

  // Dendrogram state
  const [dendrogramData, setDendrogramData] = useState<DendrogramData | null>(null);
  const [mergePhaseJson, setMergePhaseJson] = useState<string>('');
  const [selectedThreshold, setSelectedThreshold] = useState(threshold);

  // Visualization state
  const [viewMode, setViewMode] = useState<'scatter' | 'diff'>('diff');
  const [selectedCluster, setSelectedCluster] = useState(0);
  const [focusPanel, setFocusPanel] = useState<'tree' | 'diff'>('tree');
  const [selectedFilePath, setSelectedFilePath] = useState<string>('');
  const [commitShas, setCommitShas] = useState<string[]>([]);
  const [fileDiffs, setFileDiffs] = useState<Map<string, Map<string, DiffLine[]>>>(new Map());

  // Callback for FileTree selection
  const handleFileSelect = useCallback((_index: number, filepath: string) => {
    setSelectedFilePath(filepath);
  }, []);

  // Phase transition helper (handles dev mode intercepts)
  const goToPhase = useCallback((nextPhase: Phase) => {
    if (dev && nextPhase !== 'done' && nextPhase !== 'error' && nextPhase !== 'cancelled') {
      setPendingPhase(nextPhase);
      setPhase('dev-confirm');
    } else {
      setPhase(nextPhase);
    }
  }, [dev]);

  // Shared cleanup function
  const performCleanup = useCallback(async (deletePatches = true) => {
    try {
      await git.cleanup();
      if (deletePatches) {
        const fs = await import('fs/promises');
        await fs.rm('/tmp/gcommit', { recursive: true, force: true });
      }
    } catch {
      // Ignore cleanup errors
    }
  }, [git]);

  // Global cleanup on process exit/interrupt
  useEffect(() => {
    const handleExit = () => { performCleanup(phase !== 'error'); };
    const handleSignal = () => { performCleanup(phase !== 'error').then(() => process.exit(1)); };

    process.on('exit', handleExit);
    process.on('SIGINT', handleSignal);
    process.on('SIGTERM', handleSignal);

    return () => {
      process.off('exit', handleExit);
      process.off('SIGINT', handleSignal);
      process.off('SIGTERM', handleSignal);
    };
  }, [performCleanup, phase]);

  // Phase functions
  const runInit = useCallback(async () => {
    try {
      setStatusMessage('Initializing...');
      const fs = await import('fs/promises');
      await fs.rm('/tmp/gcommit', { recursive: true, force: true });
      await fs.mkdir('/tmp/gcommit', { recursive: true });

      setStatusMessage('Creating staging branch...');
      await git.createStagingBranch();

      goToPhase('processing');
    } catch (err: any) {
      setError(err.message);
      setPhase('error');
      await performCleanup(false);
    }
  }, [git, goToPhase, performCleanup]);

  // Phase 1: Run merge mode to get dendrogram
  const runProcessing = useCallback(async () => {
    try {
      setStatusMessage('Analyzing changes with AI...');

      const { execa } = await import('execa');
      const fs = await import('fs/promises');
      const diff = git.stagedDiff;

      const scriptDir = dirname(fileURLToPath(import.meta.url));
      const binaryPath = join(scriptDir, 'git_gcommit.o');
      const args = ['-m'];
      if (verbose) args.push('-v');

      const result = await execa(binaryPath, args, {
        input: diff,
        encoding: 'utf8',
      });

      if (result.stderr) {
        setStderr(result.stderr);
      }

      // Save output to temp file for phase 2
      const jsonPath = '/tmp/gcommit/state.json';
      await fs.writeFile(jsonPath, result.stdout);
      setMergePhaseJson(jsonPath);

      // Parse dendrogram data for UI
      const data: MergePhaseResult = JSON.parse(result.stdout);
      setDendrogramData(data.dendrogram);

      goToPhase('dendrogram');
    } catch (err: any) {
      setError(err.message);
      setPhase('error');
      await performCleanup(false);
    }
  }, [git.stagedDiff, verbose, goToPhase, performCleanup]);

  // Phase 2: Run threshold mode to get commits
  const runThresholdProcessing = useCallback(async () => {
    try {
      setStatusMessage('Applying threshold and generating commits...');

      const { execa } = await import('execa');

      const scriptDir = dirname(fileURLToPath(import.meta.url));
      const binaryPath = join(scriptDir, 'git_gcommit.o');
      const args = ['-t', String(selectedThreshold), mergePhaseJson];
      if (verbose) args.push('-v');

      const result = await execa(binaryPath, args, {
        encoding: 'utf8',
      });

      if (result.stderr) {
        setStderr(prev => prev + '\n' + result.stderr);
      }

      const data: { commits: ProcessingResult['commits'] } = JSON.parse(result.stdout);
      setProcessingResult({ commits: data.commits, visualization: { points: [], clusters: [] } });

      goToPhase('applying');
    } catch (err: any) {
      setError(err.message);
      setPhase('error');
      await performCleanup(false);
    }
  }, [selectedThreshold, mergePhaseJson, verbose, goToPhase, performCleanup]);

  const runApplying = useCallback(async () => {
    try {
      const data = processingResult;
      if (!data) return;

      const messages: string[] = [];
      const shas: string[] = [];

      for (let i = 0; i < data.commits.length; i++) {
        const commit = data.commits[i]!;
        setStatusMessage(`Applying cluster ${i + 1}/${data.commits.length}...`);

        for (const patchFile of commit.patch_files) {
          await git.applyPatch(patchFile);
        }

        await git.stageAll();
        await git.commit(commit.message);
        messages.push(commit.message);

        const sha = await git.git.revparse(['HEAD']);
        shas.push(sha);
      }

      setCommitMessages(messages);
      setCommitShas(shas);

      // Load file diffs for visualization
      const allDiffs = new Map<string, Map<string, DiffLine[]>>();
      for (const sha of shas) {
        const perFileDiffs = new Map<string, DiffLine[]>();
        const filesOutput = await git.git.diff([`${sha}^`, sha, '--name-only']);
        const fileList = filesOutput.trim().split('\n').filter(f => f);

        for (const file of fileList) {
          const fileDiff = await git.git.diff(['-U999999', `${sha}^`, sha, '--', file]);
          const diffLines = parseFullContextDiff(fileDiff);
          perFileDiffs.set(file, diffLines);
        }
        allDiffs.set(sha, perFileDiffs);
      }
      setFileDiffs(allDiffs);

      goToPhase('visualization');
    } catch (err: any) {
      setError(err.message);
      setPhase('error');
      await performCleanup(false);
    }
  }, [git, processingResult, goToPhase, performCleanup]);

  const runMerging = useCallback(async () => {
    try {
      setStatusMessage('Merging to original branch...');
      await git.mergeStagingBranch();
      goToPhase('restoring');
    } catch (err: any) {
      setError(err.message);
      setPhase('error');
      await performCleanup(false);
    }
  }, [git, goToPhase, performCleanup]);

  const runRestoring = useCallback(async () => {
    try {
      setStatusMessage('Cleaning up...');
      await performCleanup();
      setPhase('done');
      exit();
    } catch (err: any) {
      setError(err.message);
      setPhase('error');
    }
  }, [performCleanup, exit]);

  const runCancelled = useCallback(async () => {
    setStatusMessage('Cancelling...');
    await performCleanup();
    exit();
  }, [performCleanup, exit]);

  // Single useEffect with switch on phase
  useEffect(() => {
    switch (phase) {
      case 'init':
        runInit();
        break;
      case 'processing':
        runProcessing();
        break;
      case 'threshold-processing':
        runThresholdProcessing();
        break;
      case 'applying':
        runApplying();
        break;
      case 'merging':
        runMerging();
        break;
      case 'restoring':
        runRestoring();
        break;
      case 'cancelled':
        runCancelled();
        break;
      // 'dev-confirm', 'dendrogram', 'visualization', 'done', 'error': UI-driven, no async work
    }
  }, [phase]);

  // Keyboard input handler
  useInput((input, key) => {
    // Dev mode confirmation
    if (phase === 'dev-confirm' && pendingPhase) {
      if (input === 'y' || key.return) {
        setPhase(pendingPhase);
        setPendingPhase(null);
      } else if (input === 'n') {
        setPendingPhase(null);
        setPhase('cancelled');
      }
      return;
    }

    // Visualization phase controls
    if (phase === 'visualization') {
      const numClusters = commitShas.length;

      // View toggle
      if (input === 'v') {
        setViewMode(v => (v === 'scatter' ? 'diff' : 'scatter'));
      }

      // Panel toggle (diff view only)
      if (viewMode === 'diff' && key.tab) {
        setFocusPanel(p => (p === 'tree' ? 'diff' : 'tree'));
      }

      // Commit navigation with shift key (works from any panel)
      if (key.shift && (input === 'h' || input === 'H' || key.leftArrow)) {
        setSelectedCluster(c => Math.max(0, c - 1));
        setSelectedFilePath('');
      } else if (key.shift && (input === 'l' || input === 'L' || key.rightArrow)) {
        setSelectedCluster(c => Math.min(numClusters - 1, c + 1));
        setSelectedFilePath('');
      }

      // Apply/Cancel
      if (input === 'a') {
        goToPhase('merging');
      } else if (input === 'q' || input === 'c' || key.escape) {
        setPhase('cancelled');
      }
    }
  });

  // Render based on phase

  // Dev mode: show confirmation prompt
  if (phase === 'dev-confirm' && pendingPhase) {
    return (
      <Box flexDirection="column">
        <Text color="yellow">
          [DEV] Proceed to "{pendingPhase}"?
        </Text>
        <Text dimColor>Press y/Enter to continue, n to cancel</Text>
      </Box>
    );
  }

  if (phase === 'init' || phase === 'processing' || phase === 'threshold-processing' || phase === 'applying') {
    return (
      <Box flexDirection="column">
        <Spinner label={statusMessage} />
        {verbose && stderr && (
          <Box marginTop={1}>
            <Text dimColor>{stderr}</Text>
          </Box>
        )}
      </Box>
    );
  }

  // Dendrogram phase - user adjusts threshold
  if (phase === 'dendrogram' && dendrogramData) {
    return (
      <Dendrogram
        data={dendrogramData}
        threshold={selectedThreshold}
        onThresholdChange={setSelectedThreshold}
        onConfirm={() => goToPhase('threshold-processing')}
        onCancel={() => setPhase('cancelled')}
      />
    );
  }

  if (phase === 'visualization' && commitShas.length > 0) {
    const currentSha = commitShas[selectedCluster];
    const currentFiles = fileDiffs.get(currentSha || '');
    const fileList = currentFiles ? Array.from(currentFiles.keys()) : [];
    const currentDiff = currentFiles?.get(selectedFilePath) || [];
    const commitMessage = processingResult?.commits[selectedCluster]?.message || '';

    // Scatter view
    if (viewMode === 'scatter' && processingResult?.visualization) {
      return (
        <Box flexDirection="column">
          <ScatterPlot points={processingResult.visualization.points} />
          <ClusterLegend clusters={processingResult.visualization.clusters} />
          <Box marginTop={1}>
            <Text dimColor>h/l: commits · </Text>
            <Text color="yellow" bold>v</Text>
            <Text dimColor>: diff view · </Text>
            <Text color="green" bold>a</Text>
            <Text dimColor>: apply · </Text>
            <Text color="red" bold>q</Text>
            <Text dimColor>: quit</Text>
          </Box>
        </Box>
      );
    }

    // Diff view
    return (
      <Box flexDirection="column">
        {/* Header */}
        <Box marginBottom={1} borderStyle="single" paddingX={1}>
          <Text bold>
            Commit {selectedCluster + 1}/{commitShas.length}: {commitMessage.split('\n')[0]}
          </Text>
        </Box>

        {/* Two-panel layout */}
        <Box>
          <FileTree
            files={fileList}
            focused={focusPanel === 'tree'}
            onFileSelect={handleFileSelect}
          />
          <DiffViewer
            content={currentDiff}
            filepath={selectedFilePath}
            focused={focusPanel === 'diff'}
          />
        </Box>

        {/* Controls */}
        <Box marginTop={1}>
          <Text dimColor>
            TAB: panels · {focusPanel === 'tree' ? 'j/k: files' : 'j/k/h/l: scroll'} · shift+h/l: commits · </Text>
          <Text color="yellow" bold>v</Text>
          <Text dimColor>: scatter · </Text>
          <Text color="green" bold>a</Text>
          <Text dimColor>: apply · </Text>
          <Text color="red" bold>q</Text>
          <Text dimColor>: quit</Text>
        </Box>
      </Box>
    );
  }

  if (phase === 'merging' || phase === 'restoring') {
    return (
      <Box>
        <Spinner label={statusMessage} />
      </Box>
    );
  }

  if (phase === 'done') {
    return (
      <Box flexDirection="column">
        <Text color="green">✓ Created {commitMessages.length} commit(s) on {git.originalBranch}</Text>
        {commitMessages.map((msg, i) => (
          <Text key={i} dimColor>  - {msg.split('\n')[0]}</Text>
        ))}
      </Box>
    );
  }

  if (phase === 'cancelled') {
    return (
      <Box>
        <Text dimColor>Cancelled. No changes made.</Text>
      </Box>
    );
  }

  if (phase === 'error') {
    return (
      <Box flexDirection="column">
        <Text color="red">Error: {error}</Text>
        <Text dimColor>Cleaned up staging branch and restored stash.</Text>
      </Box>
    );
  }

  return null;
}

export default function App(props: Props) {
  return (
    <GitProvider dev={props.dev}>
      <AppContent {...props} />
    </GitProvider>
  );
}
