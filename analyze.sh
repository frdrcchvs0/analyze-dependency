#!/bin/bash

# ==============================================================================
# YOUR CUSTOM LOGIC GOES HERE
# ==============================================================================
process_file() {
    local file="$1"
    echo "$file"
}
# ==============================================================================

ANCHOR_FILE=".last_run"
IGNORE_FILE=".ignore_list"
RAW_CHANGED=$(mktemp)
FILTERED_CHANGED=$(mktemp)
AFFECTED_FILES=$(mktemp)
FINAL_LIST=$(mktemp)
VISITED=$(mktemp)

# Clean up temporary files on exit
trap 'rm -f "$RAW_CHANGED" "$FILTERED_CHANGED" "$AFFECTED_FILES" "$FINAL_LIST" "$VISITED"' EXIT

# Step 1: Find initially changed files
if [ -f "$ANCHOR_FILE" ]; then
    find . -type f -newer "$ANCHOR_FILE" ! -name ".*" | sed 's|^\./||' > "$RAW_CHANGED"
else
    find . -type f ! -name ".*" | sed 's|^\./||' > "$RAW_CHANGED"
fi

# Filter out files matching patterns in .ignore_list
if [ -f "$IGNORE_FILE" ]; then
    while IFS= read -r file || [ -n "$file" ]; do
        [ -z "$file" ] && continue
        
        should_ignore=0
        while IFS= read -r pattern || [ -n "$pattern" ]; do
            [[ -z "$pattern" || "$pattern" == "#"* ]] && continue
            clean_pattern=$(echo "$pattern" | sed 's|/$||')

            if [[ "$file" == "$clean_pattern" || "$file" == "$clean_pattern/"* ]]; then
                should_ignore=1
                break
            fi
        done < "$IGNORE_FILE"

        if [ $should_ignore -eq 0 ]; then
            echo "$file" >> "$FILTERED_CHANGED"
        fi
    done < "$RAW_CHANGED"
else
    cat "$RAW_CHANGED" > "$FILTERED_CHANGED"
fi

# Helper function to normalize paths
normalize_path() {
    echo "$1" | sed 's|/\./|/|g; s|^\./||; s|//+|/|g'
}

# Step 2: Ripple Effect (With Directory Dependency Support)
find_downstream() {
    local target="$1"
    
    if grep -qx "$target" "$AFFECTED_FILES"; then
        return
    fi
    echo "$target" >> "$AFFECTED_FILES"
    
    # Find all hidden dependency files
    find . -type f -name ".*" ! -name "$ANCHOR_FILE" | sed 's|^\./||' | while read -r dep_file; do
        local dep_dir=$(dirname "$dep_file")
        local match_found=0

        # Read each line of the dependency file to check for a match
        while IFS= read -r dep_line || [ -n "$dep_line" ]; do
            [ -z "$dep_line" ] && continue
            
            # Resolve the dependency line path relative to dep_dir
            local resolved_dep=""
            if [[ "$dep_line" == /* ]]; then
                resolved_dep="${dep_line#/}"
            else
                if [ "$dep_dir" = "." ]; then
                    resolved_dep="$dep_line"
                else
                    resolved_dep="$dep_dir/$dep_line"
                fi
            fi
            resolved_dep=$(normalize_path "$resolved_dep")

            # --- NEW: Check if the dependency is a directory or an exact file match ---
            if [ -d "$resolved_dep" ]; then
                # Clean trailing slashes just in case
                local clean_dir_dep=$(echo "$resolved_dep" | sed 's|/$||')
                # If our target changed file resides inside this directory dependency
                if [[ "$target" == "$clean_dir_dep/"* ]]; then
                    match_found=1
                    break
                fi
            elif [ "$resolved_dep" = "$target" ]; then
                # Fallback to standard exact file match
                match_found=1
                break
            fi
            # --------------------------------------------------------------------------
        done < "$dep_file"

        if [ $match_found -eq 1 ]; then
            local base=$(basename "$dep_file")
            local parent_file="${dep_dir}/${base#.}"
            parent_file=$(normalize_path "$parent_file")

            find_downstream "$parent_file"
        fi
    done
}

# Run the ripple effect for every filtered changed file
while IFS= read -r changed_file; do
    [ -z "$changed_file" ] && continue
    find_downstream "$changed_file"
done < "$FILTERED_CHANGED"


# Step 3: Topological Sort (Ordering the Output)
sort_dependencies() {
    local file="$1"
    
    if grep -qx "$file" "$VISITED"; then
        return
    fi
    echo "$file" >> "$VISITED"
    
    local dir=$(dirname "$file")
    local base=$(basename "$file")
    local dep_file="$dir/.$base"
    
    if [ -f "$dep_file" ]; then
        while IFS= read -r dep_line || [ -n "$dep_line" ]; do
            [ -z "$dep_line" ] && continue
            
            local resolved_dep=""
            if [[ "$dep_line" == /* ]]; then
                resolved_dep="${dep_line#/}"
            else
                if [ "$dir" = "." ]; then
                    resolved_dep="$dep_line"
                else
                    resolved_dep="$dir/$dep_line"
                fi
            fi
            resolved_dep=$(normalize_path "$resolved_dep")
            
            # --- NEW: Check if this directory/file dependency is in the affected pool ---
            if [ -d "$resolved_dep" ]; then
                local clean_dir_dep=$(echo "$resolved_dep" | sed 's|/$||')
                # Loop through our affected pool to see if ANY affected file comes from this directory
                while IFS= read -r affected_file; do
                    if [[ "$affected_file" == "$clean_dir_dep/"* ]]; then
                        # If a file in that directory was affected, sort that specific file first!
                        sort_dependencies "$affected_file"
                    fi
                done < "$AFFECTED_FILES"
            elif grep -qx "$resolved_dep" "$AFFECTED_FILES"; then
                sort_dependencies "$resolved_dep"
            fi
            # ----------------------------------------------------------------------------
        done < "$dep_file"
    fi
    
    if ! grep -qx "$file" "$FINAL_LIST"; then
        echo "$file" >> "$FINAL_LIST"
    fi
}

# Sort only the affected files
while IFS= read -r affected_file; do
    [ -z "$affected_file" ] && continue
    > "$VISITED"
    sort_dependencies "$affected_file"
done < "$AFFECTED_FILES"

# Process the final sorted list
while IFS= read -r sorted_file; do
    [ -z "$sorted_file" ] && continue
    process_file "$sorted_file"
done < "$FINAL_LIST"

# Update our anchor for the next run
touch "$ANCHOR_FILE"
