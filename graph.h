#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>

#define MAX_NODES 50
#define INF 999999

// Node definition (City)
typedef struct {
    int id;
    char name[100];
    int type; // 0 = Regional, 1 = Capital (has drone team)
} Node;

// Graph definition (Adjacency Matrix)
typedef struct {
    Node nodes[MAX_NODES];
    int adj[MAX_NODES][MAX_NODES]; // Stores weights (distances). INF if no edge
    int num_nodes;
    int num_edges;
} Graph;

// Function Prototypes
void init_graph(Graph *g);
int load_graph(const char *filename, Graph *g);
void print_graph(const Graph *g);

// New: Dijkstra algorithm to find nearest available drone team
// start_node: ID of the city in alert
// team_status: Array where team_status[id] == 1 means busy, 0 means free
// distance_out: Pointer to store the calculated distance (optional output)
// Returns: ID of the nearest available capital, or -1 if none found.
int find_nearest_drone(const Graph *g, int start_node, const int *team_status, int *distance_out);

#endif // GRAPH_H