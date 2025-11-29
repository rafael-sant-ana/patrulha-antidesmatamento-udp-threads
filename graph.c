#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"

// Initialize graph with infinite distances
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

// Load graph from text file
int load_graph(const char *filename, Graph *g) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening graph file");
        return -1;
    }

    init_graph(g);

    if (fscanf(file, "%d %d", &g->num_nodes, &g->num_edges) != 2) {
        fprintf(stderr, "Error reading header\n");
        fclose(file);
        return -1;
    }

    if (g->num_nodes > MAX_NODES) {
        fprintf(stderr, "Error: Too many nodes\n");
        fclose(file);
        return -1;
    }

    // Read Nodes
    for (int i = 0; i < g->num_nodes; i++) {
        int id, type;
        char name[100];
        if (fscanf(file, "%d %s %d", &id, name, &type) != 3) {
            fprintf(stderr, "Error reading node\n");
            fclose(file);
            return -1;
        }
        if (id >= 0 && id < MAX_NODES) {
            g->nodes[id].id = id;
            strncpy(g->nodes[id].name, name, sizeof(g->nodes[id].name) - 1);
            g->nodes[id].type = type;
        }
    }

    // Read Edges
    for (int i = 0; i < g->num_edges; i++) {
        int u, v, weight;
        if (fscanf(file, "%d %d %d", &u, &v, &weight) != 3) {
            fprintf(stderr, "Error reading edge\n");
            fclose(file);
            return -1;
        }
        if (u >= 0 && u < MAX_NODES && v >= 0 && v < MAX_NODES) {
            g->adj[u][v] = weight;
            g->adj[v][u] = weight;
        }
    }

    fclose(file);
    return 0;
}

void print_graph(const Graph *g) {
    printf("Graph Loaded: %d nodes, %d edges\n", g->num_nodes, g->num_edges);
}

// Dijkstra Implementation
int find_nearest_drone(const Graph *g, int start_node, const int *team_status, int *distance_out) {
    int dist[MAX_NODES];
    int visited[MAX_NODES];
    int n = g->num_nodes;

    // Initialize
    for (int i = 0; i < n; i++) {
        dist[i] = INF;
        visited[i] = 0;
    }
    dist[start_node] = 0;

    // Dijkstra Loop
    for (int count = 0; count < n - 1; count++) {
        // Pick minimum distance vertex from set of unvisited vertices
        int min = INF, u = -1;
        for (int v = 0; v < n; v++) {
            if (!visited[v] && dist[v] <= min) {
                min = dist[v];
                u = v;
            }
        }

        if (u == -1 || dist[u] == INF) break; // Remaining nodes are unreachable

        visited[u] = 1;

        // Update dist value of adjacent vertices
        for (int v = 0; v < n; v++) {
            if (!visited[v] && g->adj[u][v] != INF && dist[u] != INF && 
                dist[u] + g->adj[u][v] < dist[v]) {
                dist[v] = dist[u] + g->adj[u][v];
            }
        }
    }

    // Find nearest available capital
    int best_node = -1;
    int min_dist = INF;

    for (int i = 0; i < n; i++) {
        // Check if node is a Capital (Type 1) AND Team is Available (Status 0)
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