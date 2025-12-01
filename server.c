#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "common.h"
#include "graph.h"

Graph amazonia_graph;
int drone_teams_status[MAX_NODES]; 
int city_mission_active[MAX_NODES]; 

void send_ack(int socket_fd, struct sockaddr *dest_addr, socklen_t addr_len, int ack_type) {
    header_t header;
    payload_ack_t payload;

    header.type = htons(MSG_ACK);
    header.length = htons(sizeof(payload_ack_t));
    payload.status = htonl(ack_type); 

    
    char buffer[sizeof(header_t) + sizeof(payload_ack_t)];
    memcpy(buffer, &header, sizeof(header_t));
    memcpy(buffer + sizeof(header_t), &payload, sizeof(payload_ack_t));

    sendto(socket_fd, buffer, sizeof(buffer), 0, dest_addr, addr_len);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <v4|v6>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int ai_family = AF_UNSPEC;
    if (strcmp(argv[1], "v4") == 0) {
        ai_family = AF_INET;
    } else if (strcmp(argv[1], "v6") == 0) {
        ai_family = AF_INET6;
    } else {
        fprintf(stderr, "Erro: Argumento invalido. Use 'v4' ou 'v6'.\n");
        exit(EXIT_FAILURE);
    }
    
    if (load_graph("grafo_amazonia_legal.txt", &amazonia_graph) != 0) {
        fprintf(stderr, "Failed to load graph. Exiting.\n");
        exit(EXIT_FAILURE);
    }
    
    memset(drone_teams_status, 0, sizeof(drone_teams_status));
    memset(city_mission_active, 0, sizeof(city_mission_active));

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = ai_family;    
    hints.ai_socktype = SOCK_DGRAM; 
    hints.ai_flags = AI_PASSIVE;    

    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    
    if (res->ai_family == AF_INET6) {
        int no = 0;
        setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res);
    printf("Servidor escutando na porta %s (Modo: %s)...\n", PORT, argv[1]);

    
    char buffer[BUF_SIZE];
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        ssize_t received_bytes = recvfrom(sockfd, buffer, BUF_SIZE, 0, 
                                          (struct sockaddr *)&client_addr, &addr_len);
        
        if (received_bytes < (ssize_t)sizeof(header_t)) continue; 

        header_t *header = (header_t *)buffer;
        uint16_t msg_type = ntohs(header->type);
        uint16_t msg_len = ntohs(header->length);

        if (received_bytes < (ssize_t)(sizeof(header_t) + msg_len)) {
            printf("Warning: Packet truncated.\n");
            continue;
        }

        switch (msg_type) {
            case MSG_TELEMETRIA: {
                printf("\n[TELEMETRIA RECEBIDA]\n");
                payload_telemetria_t *telemetria = (payload_telemetria_t *)(buffer + sizeof(header_t));
                
                
                send_ack(sockfd, (struct sockaddr *)&client_addr, addr_len, ACK_TELEMETRIA);

                
                int total_cities = ntohl(telemetria->total); 
                
                printf("Total de cidades monitoradas: %d\n", total_cities);

                for (int i = 0; i < total_cities && i < MAX_CITIES; i++) {
                    int city_id = ntohl(telemetria->dados[i].id_cidade);
                    int city_status = ntohl(telemetria->dados[i].status);

                    if (city_status == 1) {
                        printf("ALERTA: %s (ID=%d)\n", amazonia_graph.nodes[city_id].name, city_id);
                        
                        
                        if (city_mission_active[city_id] == 1) {
                            printf(" -> Já existe equipe atuando em %s. Alerta ignorado.\n", amazonia_graph.nodes[city_id].name);
                            continue;
                        }

                        
                        int dist = -1;
                        int best_team = find_nearest_drone(&amazonia_graph, city_id, drone_teams_status, &dist);

                        if (best_team != -1) {
                            printf("\n[DESPACHANDO DRONES]\n");
                            printf("Cidade em alerta: %s (ID=%d)\n", amazonia_graph.nodes[city_id].name, city_id);
                            printf("> Dijkstra: Capital %s (ID=%d) selecionada, distancia = %d km\n", 
                                   amazonia_graph.nodes[best_team].name, best_team, dist);
                            
                            
                            drone_teams_status[best_team] = 1;
                            city_mission_active[city_id] = 1;

                            
                            header_t resp_header;
                            payload_equipe_drone_t resp_payload;

                            resp_header.type = htons(MSG_EQUIPE_DRONE);
                            resp_header.length = htons(sizeof(payload_equipe_drone_t));
                            
                            resp_payload.id_cidade = htonl(city_id);
                            resp_payload.id_equipe = htonl(best_team);

                            char resp_buf[sizeof(header_t) + sizeof(payload_equipe_drone_t)];
                            memcpy(resp_buf, &resp_header, sizeof(header_t));
                            memcpy(resp_buf + sizeof(header_t), &resp_payload, sizeof(payload_equipe_drone_t));

                            sendto(sockfd, resp_buf, sizeof(resp_buf), 0, (struct sockaddr *)&client_addr, addr_len);
                            printf("> Ordem enviada: Equipe %s (ID=%d) -> Cidade %s (ID=%d)\n",
                                   amazonia_graph.nodes[best_team].name, best_team, 
                                   amazonia_graph.nodes[city_id].name, city_id);
                        } else {
                            printf("ALERTA CRÍTICO: Nenhuma equipe de drones disponível para %s!\n", 
                                   amazonia_graph.nodes[city_id].name);
                        }
                    }
                }
                break;
            }

            case MSG_ACK: {
                payload_ack_t *ack = (payload_ack_t *)(buffer + sizeof(header_t));
                int status = ntohl(ack->status);
                printf("\n[ACK RECEBIDO] Status: %d\n", status);
                if (status == ACK_EQUIPE_DRONE) {
                    printf("Cliente confirmou recebimento de ordem de drone.\n");
                }
                break;
            }

            case MSG_CONCLUSAO: {
                payload_conclusao_t *conclusao = (payload_conclusao_t *)(buffer + sizeof(header_t));
                
                int city_id = ntohl(conclusao->id_cidade);
                int team_id = ntohl(conclusao->id_equipe);

                printf("\n[MISSAO CONCLUIDA]\n");
                printf("Cidade atendida: %s (ID=%d)\n", amazonia_graph.nodes[city_id].name, city_id);
                printf("Equipe: %s (ID=%d)\n", amazonia_graph.nodes[team_id].name, team_id);
                
                
                drone_teams_status[team_id] = 0;
                city_mission_active[city_id] = 0;

                printf("Equipe %s liberada para novas missoes\n", amazonia_graph.nodes[team_id].name);

                send_ack(sockfd, (struct sockaddr *)&client_addr, addr_len, ACK_CONCLUSAO);
                break;
            }

            default:
                printf("Mensagem desconhecida recebida: %d\n", msg_type);
        }
    }

    close(sockfd);
    return 0;
}