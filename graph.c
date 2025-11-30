#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"

void init_graph(Graph *g) {
    g->num_nodes = 0;
    g->num_edges = 0;
    for (int i = 0; i < MAX_NODES; i++) {
        g->nodes[i].id = -1;
        g->nodes[i].type = -1;
        memset(g->nodes[i].name, 0, sizeof(g->nodes[i].name));
        
        for (int j = 0; j < MAX_NODES; j++) {
            if (i == j) g->adj[i][j] = 0;
            else g->adj[i][j] = INF;
        }
    }
}

void trim_trailing_whitespace(char *str) {
    if (!str) return;
    int n = strlen(str);
    while (n > 0 && isspace((unsigned char)str[n - 1])) {
        str[n - 1] = '\0';
        n--;
    }
}

int load_graph(const char *filename, Graph *g) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir arquivo do grafo");
        return -1;
    }

    init_graph(g);

    char line[256];
    int header_found = 0;

    while (fgets(line, sizeof(line), file)) {
        char *p = line;
        
        if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
            p += 3;
        }

        while (isspace((unsigned char)*p)) p++;

        if (*p == '\0' || *p == '#') continue;

        if (sscanf(p, "%d %d", &g->num_nodes, &g->num_edges) == 2) {
            header_found = 1;
            break;
        } else {
            fprintf(stderr, "Erro: Formato de cabeçalho inválido. Linha lida: '%s'\n", line);
            fclose(file);
            return -1;
        }
    }

    if (!header_found) {
        fprintf(stderr, "Erro: Cabeçalho (N M) não encontrado ou arquivo vazio.\n");
        fclose(file);
        return -1;
    }

    if (g->num_nodes > MAX_NODES) {
        fprintf(stderr, "Erro: Número de nós (%d) excede o máximo permitido (%d)\n", g->num_nodes, MAX_NODES);
        fclose(file);
        return -1;
    }
    
    for (int i = 0; i < g->num_nodes; i++) {
        if (!fgets(line, sizeof(line), file)) break;
        
        int id, type;
        char *name_start;

        if (sscanf(line, "%d", &id) != 1) continue;

        name_start = line;
        while (*name_start && isdigit(*name_start)) name_start++; 
        while (*name_start && isspace(*name_start)) name_start++; 

        char *ptr = line + strlen(line) - 1;
        
        while (ptr > name_start && isspace(*ptr)) ptr--;
        
        while (ptr > name_start && isdigit(*ptr)) ptr--;
        
        if (sscanf(ptr + 1, "%d", &type) != 1) {
            fprintf(stderr, "Erro ao ler tipo do nó %d\n", id);
            continue;
        }

        char *name_end = ptr;
        while (name_end > name_start && isspace(*name_end)) name_end--;
        *(name_end + 1) = '\0';

        if (id >= 0 && id < MAX_NODES) {
            g->nodes[id].id = id;
            strncpy(g->nodes[id].name, name_start, sizeof(g->nodes[id].name) - 1);
            g->nodes[id].type = type;
        }
    }

    for (int i = 0; i < g->num_edges; i++) {
        if (!fgets(line, sizeof(line), file)) break;
        int u, v, weight;
        if (sscanf(line, "%d %d %d", &u, &v, &weight) == 3) {
            if (u >= 0 && u < MAX_NODES && v >= 0 && v < MAX_NODES) {
                g->adj[u][v] = weight;
                g->adj[v][u] = weight; 
            }
        }
    }

    fclose(file);
    return 0;
}


void print_graph(const Graph *g) {
    printf("Graph Loaded: %d nodes, %d edges\n", g->num_nodes, g->num_edges);
}

int find_nearest_drone(const Graph *g, int start_node, const int *team_status, int *distance_out) {
    int dist[MAX_NODES];
    int visited[MAX_NODES];
    int n = g->num_nodes;

    for (int i = 0; i < n; i++) {
        dist[i] = INF;
        visited[i] = 0;
    }
    dist[start_node] = 0;
    for (int count = 0; count < n - 1; count++) {
        int min = INF, u = -1;
        for (int v = 0; v < n; v++) {
            if (!visited[v] && dist[v] <= min) {
                min = dist[v];
                u = v;
            }
        }

        if (u == -1 || dist[u] == INF) break; 

        visited[u] = 1;

        for (int v = 0; v < n; v++) {
            if (!visited[v] && g->adj[u][v] != INF && dist[u] != INF && 
                dist[u] + g->adj[u][v] < dist[v]) {
                dist[v] = dist[u] + g->adj[u][v];
            }
        }
    }

    int best_node = -1;
    int min_dist = INF;

    for (int i = 0; i < n; i++) {
        // no capital ==> tipo 1, time disponivel ==> status 0
        if (g->nodes[i].type == 1 && team_status[i] == 0) {
            if (dist[i] < min_dist) {
                min_dist = dist[i];
                best_node = i;
            }
        }
    }

    if (distance_out) *distance_out = min_dist;
    return best_node;
}