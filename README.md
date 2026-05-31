A lightweight, cross-platform (`macOS` and `Linux`) shell script that performs a **Topological Sort** to resolve file dependency chains using standard filesystem mechanics.

## How it works
1. **Tracks State:** Uses a `.last_run` timestamp anchor file to only scan recently changed files.
2. **Filters Junk:** Respects a customizable `.ignore_list` file to skip directories like `.git`.
3. **Resolves Ripples:** Recursively climbs up the dependency chain using hidden dot-files (e.g., `.b` declares dependencies for `b`).
4. **Executes in Order:** Runs a custom `process_file` hook ensuring base files are processed before their dependents.
EOF
