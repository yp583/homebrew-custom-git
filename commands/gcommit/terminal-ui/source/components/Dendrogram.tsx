import React from 'react';
import {Text, Box, useInput} from 'ink';
import type {DendrogramData, MergeEvent} from '../types.js';

type Props = {
	data: DendrogramData;
	threshold: number;
	onThresholdChange: (threshold: number) => void;
	onConfirm: () => void;
	onCancel: () => void;
};

function countClustersAtThreshold(merges: MergeEvent[], numLeaves: number, threshold: number): number {
	const parent = Array.from({length: numLeaves}, (_, i) => i);

	const find = (i: number): number => {
		if (parent[i] !== i) {
			parent[i] = find(parent[i]!);
		}
		return parent[i]!;
	};

	const unite = (a: number, b: number) => {
		const rootA = find(a);
		const rootB = find(b);
		if (rootA !== rootB) {
			parent[rootA] = rootB;
		}
	};

	for (const merge of merges) {
		if (merge.distance > threshold) break;
		unite(merge.left, merge.right);
	}

	const roots = new Set<number>();
	for (let i = 0; i < numLeaves; i++) {
		roots.add(find(i));
	}
	return roots.size;
}

function truncateLabel(label: string, maxLen: number): string {
	if (label.length <= maxLen) return label.padEnd(maxLen);
	return label.slice(0, maxLen - 1) + '…';
}

type GridCell = {
	char: string;
	color?: string;
};

function renderDendrogramGrid(
	numLeaves: number,
	merges: MergeEvent[],
	maxDist: number,
	threshold: number,
	width: number
): GridCell[][] {
	const height = numLeaves;
	const grid: GridCell[][] = Array.from({length: height}, () =>
		Array.from({length: width}, () => ({char: ' '}))
	);

	if (numLeaves === 0 || merges.length === 0) return grid;

	// Track current cluster positions (y position for each original leaf)
	const clusterY: number[] = Array.from({length: numLeaves}, (_, i) => i);
	// Track which leaves are in each cluster
	const clusterMembers: number[][] = Array.from({length: numLeaves}, (_, i) => [i]);
	// Union-find parent
	const parent: number[] = Array.from({length: numLeaves}, (_, i) => i);

	const find = (i: number): number => {
		if (parent[i] !== i) parent[i] = find(parent[i]!);
		return parent[i]!;
	};

	// Draw horizontal lines from each leaf to the threshold or their first merge
	const leafMergeX: number[] = new Array(numLeaves).fill(width - 1);

	for (const merge of merges) {
		const x = Math.min(width - 1, Math.round((merge.distance / maxDist) * (width - 1)));
		const rootA = find(merge.left);
		const rootB = find(merge.right);

		// Record merge x for leaves
		for (const leaf of clusterMembers[rootA]!) {
			if (leafMergeX[leaf] === width - 1) leafMergeX[leaf] = x;
		}
		for (const leaf of clusterMembers[rootB]!) {
			if (leafMergeX[leaf] === width - 1) leafMergeX[leaf] = x;
		}

		// Merge clusters
		const membersA = clusterMembers[rootA]!;
		const membersB = clusterMembers[rootB]!;
		const yA = clusterY[rootA]!;
		const yB = clusterY[rootB]!;

		const minY = Math.min(yA, yB);
		const maxY = Math.max(yA, yB);
		const newY = (yA + yB) / 2;

		// Draw vertical connector
		const isBeforeThreshold = merge.distance <= threshold;
		const color = isBeforeThreshold ? 'green' : 'gray';

		for (let y = minY; y <= maxY; y++) {
			if (y >= 0 && y < height && x >= 0 && x < width) {
				grid[Math.round(y)]![x] = {char: '│', color};
			}
		}

		// Update union-find
		parent[rootA] = rootB;
		clusterMembers[rootB] = [...membersA, ...membersB];
		clusterY[rootB] = newY;
	}

	// Draw horizontal lines from leaves to their merge point
	for (let leaf = 0; leaf < numLeaves; leaf++) {
		const y = leaf;
		const endX = leafMergeX[leaf]!;
		for (let x = 0; x < endX; x++) {
			if (grid[y]![x]!.char === ' ') {
				grid[y]![x] = {char: '─', color: 'cyan'};
			}
		}
	}

	// Draw threshold line
	const thresholdX = Math.min(width - 1, Math.round((threshold / maxDist) * (width - 1)));
	for (let y = 0; y < height; y++) {
		const cell = grid[y]![thresholdX]!;
		if (cell.char === ' ' || cell.char === '─') {
			grid[y]![thresholdX] = {char: '┆', color: 'red'};
		} else if (cell.char === '│') {
			grid[y]![thresholdX] = {char: '┼', color: 'red'};
		}
	}

	return grid;
}

export default function Dendrogram({data, threshold, onThresholdChange, onConfirm, onCancel}: Props) {
	const numLeaves = data.labels.length;
	const clusterCount = countClustersAtThreshold(data.merges, numLeaves, threshold);

	const maxDist = data.max_distance || 1;
	const step = maxDist / 20;

	useInput((input, key) => {
		if (input === 'h' || key.leftArrow) {
			onThresholdChange(Math.max(0, threshold - step));
		} else if (input === 'l' || key.rightArrow) {
			onThresholdChange(Math.min(maxDist, threshold + step));
		} else if (key.return) {
			onConfirm();
		} else if (input === 'q' || key.escape) {
			onCancel();
		}
	});

	const labelWidth = 25;
	const treeWidth = 40;
	const maxRows = 20;
	const displayLeaves = Math.min(numLeaves, maxRows);

	// Render tree grid
	const grid = renderDendrogramGrid(displayLeaves, data.merges, maxDist, threshold, treeWidth);

	return (
		<Box flexDirection="column" padding={1}>
			<Text bold>Dendrogram - Adjust Threshold</Text>
			<Box marginY={1} />

			{/* Tree visualization */}
			<Box flexDirection="column">
				{grid.map((row, y) => (
					<Box key={y}>
						<Text>{truncateLabel(data.labels[y] || '', labelWidth)}</Text>
						<Text> </Text>
						{row.map((cell, x) => (
							<Text key={x} color={cell.color}>{cell.char}</Text>
						))}
					</Box>
				))}
			</Box>

			{numLeaves > maxRows && (
				<Text dimColor>  ... and {numLeaves - maxRows} more chunks</Text>
			)}

			<Box marginY={1} />

			{/* Distance scale */}
			<Box>
				<Text>{' '.repeat(labelWidth + 1)}</Text>
				<Text dimColor>0</Text>
				<Text dimColor>{' '.repeat(treeWidth - 6)}</Text>
				<Text dimColor>{maxDist.toFixed(2)}</Text>
			</Box>

			<Box marginY={1} />

			<Box>
				<Text bold>Threshold: </Text>
				<Text color="yellow">{threshold.toFixed(3)}</Text>
				<Text> │ </Text>
				<Text bold>Clusters: </Text>
				<Text color="green">{clusterCount}</Text>
			</Box>

			<Box marginTop={1}>
				<Text dimColor>h/←: decrease │ l/→: increase │ </Text>
				<Text color="green" bold>Enter</Text>
				<Text dimColor>: confirm │ </Text>
				<Text color="red" bold>q</Text>
				<Text dimColor>: cancel</Text>
			</Box>
		</Box>
	);
}
