#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <time.h>

#define MAX_PATH 1024
#define MAX_FILES 5000

// ==============================================================================
// SUA LÓGICA DE NEGÓCIO VAI AQUI
// ==============================================================================
void process_file(const char *filename) {
    printf("%s\n", filename);
}
// ==============================================================================

const char *ANCHOR_FILE = ".last_run";
const char *IGNORE_FILE = ".ignore_list";

char ignore_patterns[100][MAX_PATH];
int ignore_count = 0;

char affected_files[MAX_FILES][MAX_PATH];
int affected_count = 0;

char visited_files[MAX_FILES][MAX_PATH];
int visited_count = 0;

char final_list[MAX_FILES][MAX_PATH];
int final_count = 0;

time_t last_run_time = 0;

void load_ignore_list() {
    FILE *f = fopen(IGNORE_FILE, "r");
    if (!f) return;

    char line[MAX_PATH];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0 || line[0] == '#') continue;
        
        if (line[strlen(line) - 1] == '/') {
            line[strlen(line) - 1] = 0;
        }
        strcpy(ignore_patterns[ignore_count++], line);
    }
    fclose(f);
}

bool should_ignore(const char *path) {
    if (strncmp(path, "./", 2) == 0) path += 2;
    
    for (int i = 0; i < ignore_count; i++) {
        if (strcmp(path, ignore_patterns[i]) == 0) return true;
        
        char dir_pattern[MAX_PATH];
        snprintf(dir_pattern, sizeof(dir_pattern), "%s/", ignore_patterns[i]);
        if (strncmp(path, dir_pattern, strlen(dir_pattern)) == 0) return true;
    }
    return false;
}

void add_to_affected(const char *path) {
    for (int i = 0; i < affected_count; i++) {
        if (strcmp(affected_files[i], path) == 0) return;
    }
    strcpy(affected_files[affected_count++], path);
}

bool is_affected(const char *path) {
    for (int i = 0; i < affected_count; i++) {
        if (strcmp(affected_files[i], path) == 0) return true;
    }
    return false;
}

// Normaliza caminhos removendo o "./" repetido ou inicial
void normalize_path(const char *src, char *dst) {
    const char *p = src;
    if (strncmp(p, "./", 2) == 0) p += 2;
    strcpy(dst, p);
}

void find_changed_files(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (entry->d_name[0] == '.') continue;

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        char clean_path[MAX_PATH];
        normalize_path(path, clean_path);

        if (should_ignore(clean_path)) continue;

        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                find_changed_files(path);
            } else if (S_ISREG(st.st_mode)) {
                if (last_run_time == 0 || st.st_mtime > last_run_time) {
                    add_to_affected(clean_path);
                }
            }
        }
    }
    closedir(dir);
}

bool is_file_in_dep_directory(const char *resolved_dep, const char *target_file) {
    char clean_dep[MAX_PATH];
    strcpy(clean_dep, resolved_dep);
    
    size_t len = strlen(clean_dep);
    if (len > 0 && clean_dep[len - 1] == '/') {
        clean_dep[len - 1] = 0;
    }
    
    if (strcmp(clean_dep, ".") == 0 || strcmp(clean_dep, "") == 0) return true;

    char dir_prefix[MAX_PATH];
    snprintf(dir_prefix, sizeof(dir_prefix), "%s/", clean_dep);
    
    if (strncmp(target_file, dir_prefix, strlen(dir_prefix)) == 0) {
        return true;
    }
    return false;
}

// Rastreia os arquivos ocultos que apontam para o "target" atual
void scan_dependencies_for_target(const char *dir_path, const char *target) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                scan_dependencies_for_target(path, target);
            } else if (S_ISREG(st.st_mode) && entry->d_name[0] == '.' && strcmp(entry->d_name, ANCHOR_FILE) != 0) {
                
                // Processa o arquivo oculto encontrado
                FILE *f = fopen(path, "r");
                if (f) {
                    char line[MAX_PATH];
                    char dep_dir[MAX_PATH];
                    normalize_path(dir_path, dep_dir);

                    bool match_found = false;
                    while (fgets(line, sizeof(line), f)) {
                        line[strcspn(line, "\r\n")] = 0;
                        if (strlen(line) == 0 || line[0] == '#') continue;

                        char resolved_dep[MAX_PATH];
                        if (line[0] == '/') {
                            strcpy(resolved_dep, line + 1);
                        } else {
                            if (strcmp(dep_dir, ".") == 0 || strcmp(dep_dir, "") == 0) {
                                snprintf(resolved_dep, sizeof(resolved_dep), "%s", line);
                            } else {
                                snprintf(resolved_dep, sizeof(resolved_dep), "%s/%s", dep_dir, line);
                            }
                        }
                        
                        char clean_resolved[MAX_PATH];
                        normalize_path(resolved_dep, clean_resolved);

                        if (is_file_in_dep_directory(clean_resolved, target) || strcmp(clean_resolved, target) == 0) {
                            match_found = true;
                            break;
                        }
                    }
                    fclose(f);

                    if (match_found) {
                        // Descobre o nome do arquivo associado a este .arquivo
                        char parent_file[MAX_PATH];
                        char clean_parent[MAX_PATH];
                        
                        // Substitui o "." do início do nome do arquivo
                        if (strcmp(dep_dir, ".") == 0 || strcmp(dep_dir, "") == 0) {
                            snprintf(parent_file, sizeof(parent_file), "%s", entry->d_name + 1);
                        } else {
                            snprintf(parent_file, sizeof(parent_file), "%s/%s", dep_dir, entry->d_name + 1);
                        }
                        normalize_path(parent_file, clean_parent);

                        // Se o pai encontrado ainda não estiver marcado como afetado, adiciona e propaga!
                        if (!is_affected(clean_parent)) {
                            add_to_affected(clean_parent);
                            // Continua o efeito cascata a partir do novo pai gerado
                            scan_dependencies_for_target(".", clean_parent);
                        }
                    }
                }
            }
        }
    }
    closedir(dir);
}

void sort_dependencies(const char *file) {
    for (int i = 0; i < visited_count; i++) {
        if (strcmp(visited_files[i], file) == 0) return;
    }
    strcpy(visited_files[visited_count++], file);

    // Reconstrói o caminho do arquivo oculto correspondente (.nome)
    char dep_file[MAX_PATH];
    char clean_file[MAX_PATH];
    normalize_path(file, clean_file);
    
    char *last_slash = strrchr(clean_file, '/');
    if (last_slash) {
        *last_slash = 0;
        snprintf(dep_file, sizeof(dep_file), "%s/.%s", clean_file, last_slash + 1);
        *last_slash = '/'; // restaura
    } else {
        snprintf(dep_file, sizeof(dep_file), ".%s", clean_file);
    }

    FILE *f = fopen(dep_file, "r");
    if (f) {
        char line[MAX_PATH];
        char dir_part[MAX_PATH];
        strcpy(dir_part, clean_file);
        char *slash = strrchr(dir_part, '/');
        if (slash) *slash = 0;
        else strcpy(dir_part, ".");

        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (strlen(line) == 0 || line[0] == '#') continue;

            char resolved_dep[MAX_PATH];
            if (line[0] == '/') {
                strcpy(resolved_dep, line + 1);
            } else {
                if (strcmp(dir_part, ".") == 0) snprintf(resolved_dep, sizeof(resolved_dep), "%s", line);
                else snprintf(resolved_dep, sizeof(resolved_dep), "%s/%s", dir_part, line);
            }
            
            char clean_resolved[MAX_PATH];
            normalize_path(resolved_dep, clean_resolved);

            // Se for diretório na ordenação topográfica
            char dir_prefix[MAX_PATH];
            snprintf(dir_prefix, sizeof(dir_prefix), "%s/", clean_resolved);

            bool is_dir_match = false;
            for (int i = 0; i < affected_count; i++) {
                if (strncmp(affected_files[i], dir_prefix, strlen(dir_prefix)) == 0) {
                    is_dir_match = true;
                    sort_dependencies(affected_files[i]);
                }
            }

            if (!is_dir_match && is_affected(clean_resolved)) {
                sort_dependencies(clean_resolved);
            }
        }
        fclose(f);
    }

    for (int i = 0; i < final_count; i++) {
        if (strcmp(final_list[i], file) == 0) return;
    }
    strcpy(final_list[final_count++], file);
}

int main() {
    struct stat st;
    if (stat(ANCHOR_FILE, &st) == 0) {
        last_run_time = st.st_mtime;
    }

    load_ignore_list();

    // Passo 1: Captura os alterados físicos
    char temp_affected[MAX_FILES][MAX_PATH];
    find_changed_files(".");

    int initial_count = affected_count;
    for(int i = 0; i < initial_count; i++) {
        strcpy(temp_affected[i], affected_files[i]);
    }

    // Passo 2: Executa o Ripple Effect varrendo a partir de cada um dos alterados
    for (int i = 0; i < initial_count; i++) {
        scan_dependencies_for_target(".", temp_affected[i]);
    }

    // Passo 3: Ordenação Topológica dos afetados acumulados
    for (int i = 0; i < affected_count; i++) {
        visited_count = 0;
        sort_dependencies(affected_files[i]);
    }

    // Processamento Final
    for (int i = 0; i < final_count; i++) {
        process_file(final_list[i]);
    }

    FILE *anchor = fopen(ANCHOR_FILE, "w");
    if (anchor) fclose(anchor);

    return 0;
}
