A lightweight, cross-platform (`macOS` and `Linux`) shell script or C source, that performs a **Topological Sort** to resolve file dependency chains using standard filesystem mechanics.

## How it works
1. **Tracks State:** Uses a `.last_run` timestamp anchor file to only scan recently changed files.
2. **Filters Junk:** Respects a customizable `.ignore_list` file to skip directories like `.git`.
3. **Resolves Ripples:** Recursively climbs up the dependency chain using hidden dot-files (e.g., `.b` declares dependencies for `b`).
4. **Executes in Order:** Runs a custom `process_file` hook ensuring base files are processed before their dependents.
EOF

## pre-analyze
Since it is possible to change the contents of a 'dependency file', it is necessary to run pre-analyze before analyze. Analyze deals with source files only, pre-analyze deals with dependency only files.
