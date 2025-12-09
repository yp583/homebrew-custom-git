import React from 'react';
import {Text, Box, useInput} from 'ink';
import type {DendrogramData, MergeEvent} from '../types.js';
import {addConnection, removeConnection} from '../utils/consts.js';

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

function computeOptimalLeafOrder(merges: MergeEvent[], numLeaves: number): number[] {
	if (numLeaves === 0) return [];
	if (merges.length === 0) return Array.from({length: numLeaves}, (_, i) => i);

	const clusterLeaves: Map<number, number[]> = new Map();
	for (let i = 0; i < numLeaves; i++) {
		clusterLeaves.set(i, [i]);
	}

	const parent: number[] = Array.from({length: numLeaves}, (_, i) => i);
	const find = (i: number): number => {
		if (parent[i] !== i) parent[i] = find(parent[i]!);
		return parent[i]!;
	};

	for (const merge of merges) {
		const rootA = find(merge.left);
		const rootB = find(merge.right);

		const leavesA = clusterLeaves.get(rootA)!;
		const leavesB = clusterLeaves.get(rootB)!;

		const merged = [...leavesA, ...leavesB];

		parent[rootA] = rootB;
		clusterLeaves.delete(rootA);
		clusterLeaves.set(rootB, merged);
	}

	return [...clusterLeaves.values()][0] || [];
}

type GridCell = {
	char: string;
	color?: string;
};

type ClusterInfo = {
	trunkY: number;  // Row where outgoing trunk is drawn
	minY: number;    // Min row of all members
	maxY: number;    // Max row of all members
	lastX: number;
	members: number[];
	isLeaf: boolean; // True if this cluster hasn't merged yet (no vertical connector at lastX)
};

function drawHorizontalLine(
	grid: GridCell[][],
	y: number,
	fromX: number,
	toX: number,
	color: string,
	addRightAtStart: boolean = true
): void {
	if (y < 0 || y >= grid.length || fromX >= toX) return;

	for (let x = fromX; x < toX; x++) {
		const cell = grid[y]![x]!;
		// Add left connection if not at start
		if (x > fromX) {
			cell.char = addConnection(cell.char, 'left');
		}
		// Add right connection to continue the line
		if (x > fromX || addRightAtStart) {
			cell.char = addConnection(cell.char, 'right');
		}
		if (!cell.color) cell.color = color;
	}
	// Connect to the merge point
	grid[y]![toX]!.char = addConnection(grid[y]![toX]!.char, 'left');
}

function drawVerticalConnector(
	grid: GridCell[][],
	x: number,
	topY: number,
	bottomY: number,
	color: string
): void {
	const height = grid.length;

	for (let y = Math.max(0, topY); y <= Math.min(height - 1, bottomY); y++) {
		const cell = grid[y]![x]!;
		if (y > topY) cell.char = addConnection(cell.char, 'up');
		if (y < bottomY) cell.char = addConnection(cell.char, 'down');
		cell.color = color;
	}
}

function renderDendrogramGrid(
	numLeaves: number,
	displayRows: number,
	merges: MergeEvent[],
	maxDist: number,
	threshold: number,
	width: number,
	leafOrder: number[]
): GridCell[][] {
	const height = displayRows;
	const grid: GridCell[][] = Array.from({length: height}, () =>
		Array.from({length: width}, () => ({char: ' '}))
	);

	if (numLeaves === 0 || merges.length === 0) return grid;

	// Create mapping from original leaf index to row position
	const leafToRow: Map<number, number> = new Map();
	for (let row = 0; row < leafOrder.length; row++) {
		leafToRow.set(leafOrder[row]!, row);
	}

	const clusters: Map<number, ClusterInfo> = new Map();
	for (let i = 0; i < numLeaves; i++) {
		const row = leafToRow.get(i) ?? i;
		clusters.set(i, {trunkY: row, minY: row, maxY: row, lastX: 0, members: [i], isLeaf: true});
	}

	const parent: number[] = Array.from({length: numLeaves}, (_, i) => i);
	const find = (i: number): number => {
		if (parent[i] !== i) parent[i] = find(parent[i]!);
		return parent[i]!;
	};

	for (const merge of merges) {
		const x = Math.min(width - 1, Math.round((merge.distance / maxDist) * (width - 1)));
		const rootA = find(merge.left);
		const rootB = find(merge.right);
		const clusterA = clusters.get(rootA)!;
		const clusterB = clusters.get(rootB)!;

		const color = merge.distance <= threshold ? 'green' : 'cyan';
		const yA = clusterA.trunkY;
		const yB = clusterB.trunkY;
		const topY = Math.min(yA, yB);
		const bottomY = Math.max(yA, yB);
		const newTrunkY = Math.floor((topY + bottomY) / 2);

		// Draw horizontal lines - always add right at start to create the connection
		drawHorizontalLine(grid, yA, clusterA.lastX, x, color, true);
		drawHorizontalLine(grid, yB, clusterB.lastX, x, color, true);
		drawVerticalConnector(grid, x, topY, bottomY, color);

		// Clean up corners - remove right from top and bottom (corners should be clean)
		if (topY >= 0 && topY < height) {
			grid[topY]![x]!.char = removeConnection(grid[topY]![x]!.char, 'right');
		}
		if (bottomY >= 0 && bottomY < height) {
			grid[bottomY]![x]!.char = removeConnection(grid[bottomY]![x]!.char, 'right');
		}

		parent[rootA] = rootB;
		clusters.delete(rootA);
		clusters.set(rootB, {
			trunkY: newTrunkY,
			minY: Math.min(clusterA.minY, clusterB.minY),
			maxY: Math.max(clusterA.maxY, clusterB.maxY),
			lastX: x,
			members: [...clusterA.members, ...clusterB.members],
			isLeaf: false
		});
	}

	// Draw final trunk extending from the last merge point
	for (const [, cluster] of clusters) {
		const y = cluster.trunkY;
		if (y >= 0 && y < height && cluster.lastX < width - 1) {
			// Add right exit at the merge point
			grid[y]![cluster.lastX]!.char = addConnection(grid[y]![cluster.lastX]!.char, 'right');
			// Draw one cell extension
			const nextX = cluster.lastX + 1;
			if (nextX < width) {
				grid[y]![nextX]!.char = addConnection(grid[y]![nextX]!.char, 'left');
				if (!grid[y]![nextX]!.color) grid[y]![nextX]!.color = 'cyan';
			}
		}
	}

	// Draw threshold line as visual overlay
	const thresholdX = Math.min(width - 1, Math.round((threshold / maxDist) * (width - 1)));
	for (let y = 0; y < height; y++) {
		const cell = grid[y]![thresholdX]!;
		if (cell.char === ' ') {
			grid[y]![thresholdX] = {char: '┆', color: 'red'};
		} else {
			// Overlay on existing character - keep structure but mark threshold
			cell.color = 'red';
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

	const leafOrder = computeOptimalLeafOrder(data.merges, numLeaves);
	const grid = renderDendrogramGrid(numLeaves, displayLeaves, data.merges, maxDist, threshold, treeWidth, leafOrder);

	return (
		<Box flexDirection="column" padding={1}>
			<Text bold>Dendrogram - Adjust Threshold</Text>
			<Box marginY={1} />

			<Box flexDirection="column">
				{grid.map((row, y) => (
					<Box key={y}>
						<Text>{truncateLabel(data.labels[leafOrder[y]!] || '', labelWidth)}</Text>
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
