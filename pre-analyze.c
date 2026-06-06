#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>

// --- SEPARAÇÃO DE DEPENDÊNCIAS POR SISTEMA OPERACIONAL ---
#if defined(__APPLE__)
    #include <sys/time.h> // Necessário para utimes no macOS
#elif defined(__linux__)
    #include <utime.h>    // Equivalente padrão no Linux
#endif
// ---------------------------------------------------------

#define MAX_PATH 1024

const char *ANCHOR_FILE = ".last_run";

// Função para dar o "touch" de forma nativa e limpa
void native_touch(const char *filename) {
#if defined(__APPLE__)
    // No Mac, utimes usa NULL para carimbar com o horário atual de microsegundos
    utimes(filename, NULL);
#elif defined(__linux__)
    // No Linux, utime usa NULL para carimbar com o horário atual de segundos
    utime(filename, NULL);
#else
    // Fallback caso seja compilado em outro Unix genérico
    struct stat st;
    if (stat(filename, &st) == 0) {
        FILE *f = fopen(filename, "r+");
        if (f) {
            int ch = fgetc(f);
            if (ch != EOF) {
                fseek(f, 0, SEEK_SET);
                fputc(ch, f);
            }
            fclose(f);
        }
    }
#endif
}

void check_dependency_integrity(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        struct stat st_dep;
        if (stat(path, &st_dep) == 0) {
            if (S_ISDIR(st_dep.st_mode)) {
                check_dependency_integrity(path);
            } 
            else if (S_ISREG(st_dep.st_mode) && entry->d_name[0] == '.' && strcmp(entry->d_name, ANCHOR_FILE) != 0) {
                
                char parent_file[MAX_PATH];
                if (strcmp(dir_path, ".") == 0 || strcmp(dir_path, "") == 0) {
                    snprintf(parent_file, sizeof(parent_file), "%s", entry->d_name + 1);
                } else {
                    snprintf(parent_file, sizeof(parent_file), "%s/%s", dir_path, entry->d_name + 1);
                }

                struct stat st_parent;
                if (stat(parent_file, &st_parent) == 0 && S_ISREG(st_parent.st_mode)) {
                    if (st_dep.st_mtime > st_parent.st_mtime) {
                        printf("Regra alterada detectada: %s é mais recente que %s\n", path, parent_file);
                        
                        // Chama a nossa função isolada condicional
                        native_touch(parent_file);
                    }
                }
            }
        }
    }
    closedir(dir);
}

int main() {
    printf("=== Verificando Alterações em Arquivos de Dependência (C) ===\n");
    check_dependency_integrity(".");
    printf("=== Verificação Concluída ===\n");
    return 0;
}
