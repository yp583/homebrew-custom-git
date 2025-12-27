import React from 'react';
import { render } from 'ink';
import meow from 'meow';
import { simpleGit } from 'simple-git';
import App from './app.js';

const cli = meow(`
  Usage
    $ git gcommit [options]

  Options
    -d, --threshold  Clustering distance threshold (default: 0.5)
    -v, --verbose    Show verbose output from C++ binary
    --dev            Step through phases with confirmation prompts
    --recover        Recover changes from a failed gcommit run
    -h, --help       Show this help message

  Examples
    $ git gcommit
    $ git gcommit -d 0.3
    $ git gcommit --threshold 0.7 --verbose
    $ git gcommit --recover
`, {
  importMeta: import.meta,
  flags: {
    threshold: {
      type: 'number',
      shortFlag: 'd',
      default: 0.5,
    },
    verbose: {
      type: 'boolean',
      shortFlag: 'v',
      default: false,
    },
    dev: {
      type: 'boolean',
      default: false,
    },
    recover: {
      type: 'boolean',
      default: false,
    },
    help: {
      type: 'boolean',
      shortFlag: 'h',
    },
  },
});

// Show help and exit if requested
if (cli.flags.help) {
  cli.showHelp();
}

async function findStashByName(git: ReturnType<typeof simpleGit>, name: string): Promise<string | null> {
  const result = await git.stash(['list']);
  const lines = result.split('\n');
  for (const line of lines) {
    if (line.includes(name)) {
      const match = line.match(/^(stash@\{\d+\})/);
      if (match && match[1]) return match[1];
    }
  }
  return null;
}

async function recover() {
  const git = simpleGit();

  const isRepo = await git.checkIsRepo();
  if (!isRepo) {
    console.error('Error: Not in a git repository');
    process.exit(1);
  }

  let recovered = false;

  // Try to recover staged changes
  const stagedRef = await findStashByName(git, 'gcommit-staged');
  if (stagedRef) {
    try {
      await git.stash(['apply', '--index', stagedRef]);
      console.log('✓ Recovered staged changes');
      recovered = true;
    } catch (err: any) {
      console.error(`Failed to recover staged changes: ${err.message}`);
    }
  }

  // Try to recover unstaged changes
  const unstagedRef = await findStashByName(git, 'gcommit-unstaged');
  if (unstagedRef) {
    try {
      await git.stash(['apply', unstagedRef]);
      console.log('✓ Recovered unstaged changes');
      recovered = true;
    } catch (err: any) {
      console.error(`Failed to recover unstaged changes: ${err.message}`);
    }
  }

  if (!recovered) {
    console.log('No gcommit stashes found to recover.');
  } else {
    console.log('\nRecovery complete. Your changes have been restored.');
  }

  process.exit(0);
}

// Handle --recover flag (exits after recovery)
if (cli.flags.recover) {
  recover().catch(err => {
    console.error('Recovery failed:', err.message);
    process.exit(1);
  });
} else {

async function main() {
  const git = simpleGit();

  // Validation: Check if in a git repository
  const isRepo = await git.checkIsRepo();
  if (!isRepo) {
    console.error('Error: Not in a git repository');
    process.exit(1);
  }

  // Validation: Check for staged changes
  const staged = await git.diff(['--cached', '--name-only']);
  if (!staged.trim()) {
    console.error('Error: No staged changes. Use `git add` to stage files.');
    process.exit(1);
  }

  // Launch Ink app
  const { waitUntilExit } = render(
    <App
      threshold={cli.flags.threshold}
      verbose={cli.flags.verbose}
      dev={cli.flags.dev}
    />,
    { exitOnCtrlC: false }
  );

  await waitUntilExit();
}

main().catch(err => {
  console.error('Error:', err.message);
  console.error('\nIf changes were lost, try: git gcommit --recover');
  console.error('Or manually: git stash apply stash@{n} (check git stash list for gcommit-staged/gcommit-unstaged)');
  process.exit(1);
});
}
