#!/bin/bash

# Arquivo de ancoragem global do sistema (para ignorar na busca)
ANCHOR_FILE=".last_run"

echo "=== Verificando Alterações em Arquivos de Dependência ==="

# Busca todos os arquivos ocultos recursivamente
find . -type f -name ".*" | while read -r dep_file; do
    # Ignora o arquivo .last_run
    base_dep=$(basename "$dep_file")
    if [ "$base_dep" = "$ANCHOR_FILE" ]; then
        continue
    fi

    dep_dir=$(dirname "$dep_file")
    
    # Reconstrói o nome do arquivo correspondente (.nome -> nome)
    # Remove o ponto inicial do nome do arquivo
    parent_name="${base_dep#.}"
    
    if [ "$dep_dir" = "." ]; then
        parent_file="$parent_name"
    else
        parent_file="$dep_dir/$parent_name"
    fi

    # Se o arquivo correspondente existir, compara as datas de modificação
    if [ -f "$parent_file" ]; then
        if [ "$dep_file" -nt "$parent_file" ]; then
            echo "Regra alterada detectada: $dep_file é mais recente que $parent_file"
            # Dá o touch no arquivo correspondente para forçar o motor principal a processá-lo
            touch "$parent_file"
        fi
    fi
done

echo "=== Verificação Concluída ==="
