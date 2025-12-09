export type BoxConnections = {
	up: boolean;
	down: boolean;
	left: boolean;
	right: boolean;
};

export const BOX_CHAR_CONNECTIONS: Record<string, BoxConnections> = {
	' ': {up: false, down: false, left: false, right: false},
	'─': {up: false, down: false, left: true, right: true},
	'│': {up: true, down: true, left: false, right: false},
	'┐': {up: false, down: true, left: true, right: false},
	'┘': {up: true, down: false, left: true, right: false},
	'┌': {up: false, down: true, left: false, right: true},
	'└': {up: true, down: false, left: false, right: true},
	'┤': {up: true, down: true, left: true, right: false},
	'├': {up: true, down: true, left: false, right: true},
	'┬': {up: false, down: true, left: true, right: true},
	'┴': {up: true, down: false, left: true, right: true},
	'┼': {up: true, down: true, left: true, right: true},
};

export function getConnections(char: string): BoxConnections {
	return BOX_CHAR_CONNECTIONS[char] || {up: false, down: false, left: false, right: false};
}

export function getCharFromConnections(c: BoxConnections): string {
	const {up, down, left, right} = c;
	if (up && down && left && right) return '┼';
	if (up && down && left) return '┤';
	if (up && down && right) return '├';
	if (up && left && right) return '┴';
	if (down && left && right) return '┬';
	if (up && down) return '│';
	if (left && right) return '─';
	if (down && left) return '┐';
	if (down && right) return '┌';
	if (up && left) return '┘';
	if (up && right) return '└';
	if (up || down) return '│';
	if (left || right) return '─';
	return ' ';
}

export function addConnection(existing: string, direction: 'up' | 'down' | 'left' | 'right'): string {
	const conn = {...getConnections(existing)};
	conn[direction] = true;
	return getCharFromConnections(conn);
}

export function removeConnection(existing: string, direction: 'up' | 'down' | 'left' | 'right'): string {
	const conn = {...getConnections(existing)};
	conn[direction] = false;
	return getCharFromConnections(conn);
}
