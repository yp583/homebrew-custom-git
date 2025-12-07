import React, {useState, useEffect} from 'react';
import {Text, Box, useInput} from 'ink';

type Props = {
	files: string[];
	focused: boolean;
	maxHeight?: number;
	onFileSelect?: (index: number, filepath: string) => void;
};

const truncateWithEllipsis = (text: string, maxLength: number): string => {
	if (text.length <= maxLength) return text;
	return text.slice(0, maxLength - 1) + '…';
};

export default function FileTree({files, focused, maxHeight = 20, onFileSelect}: Props) {
	const [selectedIndex, setSelectedIndex] = useState(0);

	// Reset selection when files actually change (not just array reference)
	const filesKey = files.join(',');
	useEffect(() => {
		setSelectedIndex(0);
	}, [filesKey]);

	// Notify parent when selection changes
	useEffect(() => {
		if (files.length > 0 && onFileSelect) {
			onFileSelect(selectedIndex, files[selectedIndex] || '');
		}
	}, [selectedIndex, files, onFileSelect]);

	// Handle j/k navigation
	useInput((input, key) => {
		if (!focused) return;
		if (input === 'j' || key.downArrow) {
			setSelectedIndex(i => Math.min(files.length - 1, i + 1));
		} else if (input === 'k' || key.upArrow) {
			setSelectedIndex(i => Math.max(0, i - 1));
		}
	});
	// Pad to fixed height to prevent re-render jitter
	const paddedFiles: Array<string | null> = [];
	for (let i = 0; i < maxHeight; i++) {
		if (i < files.length) {
			paddedFiles.push(files[i]!);
		} else {
			paddedFiles.push(null);
		}
	}

	return (
		<Box
			flexDirection="column"
			width={25}
			borderStyle="single"
			borderColor={focused ? 'cyan' : 'gray'}
			paddingX={1}
			height={maxHeight + 4}
		>
			<Text bold>Changed Files</Text>
			<Box marginTop={1} flexDirection="column">
				{paddedFiles.map((file, i) => {
					if (!file) {
						return (
							<Box key={i}>
								<Text> </Text>
							</Box>
						);
					}
					const filename = file.split('/').pop() || file;
					const isSelected = i === selectedIndex;
					return (
						<Box key={i}>
							<Text
								color={isSelected ? 'cyan' : undefined}
								inverse={isSelected}
								wrap="truncate-end"
							>
								{isSelected ? '▶ ' : '  '}
								{truncateWithEllipsis(filename, 19)}
							</Text>
						</Box>
					);
				})}
			</Box>
			<Text dimColor>{files.length} file(s)</Text>
		</Box>
	);
}
