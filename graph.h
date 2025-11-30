#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>

#define MAX_NODES 50
#define INF 999999

typedef struct {
    int id;
    char name[100];
    int type; // 0 = regional, 1 = capital
} Node;

typedef struct {
    Node nodes[MAX_NODES];
    int adj[MAX_NODES][MAX_NODES];
    int num_nodes;
    int num_edges;
} Graph;

void init_graph(Graph *g);
int load_graph(const char *filename, Graph *g);
void print_graph(const Graph *g);

int find_nearest_drone(const Graph *g, int start_node, const int *team_status, int *distance_out);

#endif // GRAPH_H