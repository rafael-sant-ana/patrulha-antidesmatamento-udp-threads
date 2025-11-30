#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <netdb.h>
#include "common.h"
#include "graph.h"


Graph amazonia_map;

int current_status[MAX_NODES];
pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;

int sockfd;
struct sockaddr_storage server_addr;
socklen_t server_addr_len;
pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER; // Protects sendto()

pthread_cond_t cond_telemetry_ack = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_telemetry_ack = PTHREAD_MUTEX_INITIALIZER;
int telemetry_ack_received = 0;

typedef struct {
    int city_id;
    int team_id;
    int active; // 1 ise missao pendente
} Mission;

Mission current_mission = { -1, -1, 0 };
pthread_cond_t cond_mission_start = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_mission = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_conclusao_ack = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_conclusao_ack = PTHREAD_MUTEX_INITIALIZER;
int conclusao_ack_received = 0;


void send_udp_packet(void *buffer, size_t len) {
    pthread_mutex_lock(&socket_mutex);
    sendto(sockfd, buffer, len, 0, (struct sockaddr *)&server_addr, server_addr_len);
    pthread_mutex_unlock(&socket_mutex);
}

//---- Thread 1: simulacao de monitoramento ----
void *thread_monitoring(void *arg) {
    printf("[Thread Monitoramento] Iniciada\n");
    srand(time(NULL));

    while (1) {
        sleep(5); 

        pthread_mutex_lock(&status_mutex);
        
        int alerts = 0;
        for (int i = 0; i < amazonia_map.num_nodes; i++) {
            if ((rand() % 100) < 3) {
                current_status[i] = 1;
                alerts++;
                // printf("SENSOR: Alerta detectado em %s (ID=%d)\n", amazonia_map.nodes[i].name, i);
            } else {
                current_status[i] = 0; 
            }
        }
        pthread_mutex_unlock(&status_mutex);
    }
    return NULL;
}

//---- Thread 2: envio de telemetria ----
void *thread_telemetry(void *arg) {
    printf("[Thread Telemetria] Iniciada\n");

    while (1) {
        sleep(10);

        printf("\n[ENVIANDO TELEMETRIA]\n");

        header_t header;
        payload_telemetria_t payload;
        
        header.type = htons(MSG_TELEMETRIA);
        header.length = htons(sizeof(payload_telemetria_t));
        
        pthread_mutex_lock(&status_mutex);
        payload.total = htonl(amazonia_map.num_nodes);
        
        printf("Total de cidades: %d\n", amazonia_map.num_nodes);
        for (int i = 0; i < amazonia_map.num_nodes; i++) {
            payload.dados[i].id_cidade = htonl(amazonia_map.nodes[i].id);
            payload.dados[i].status = htonl(current_status[i]);
            
            if (current_status[i] == 1) {
                printf("ALERTA: %s (ID=%d)\n", amazonia_map.nodes[i].name, i);
            }
        }
        pthread_mutex_unlock(&status_mutex);

        char buffer[sizeof(header_t) + sizeof(payload_telemetria_t)];
        memcpy(buffer, &header, sizeof(header_t));
        memcpy(buffer + sizeof(header_t), &payload, sizeof(payload_telemetria_t));

        send_udp_packet(buffer, sizeof(buffer));

        pthread_mutex_lock(&mutex_telemetry_ack);
        struct timeval now;
        struct timespec timeout;
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec + 5; 
        timeout.tv_nsec = now.tv_usec * 1000;

        while (telemetry_ack_received == 0) {
            int rc = pthread_cond_timedwait(&cond_telemetry_ack, &mutex_telemetry_ack, &timeout);
            if (rc != 0) {
                printf("Timeout aguardando ACK de telemetria.\n");
                break;
            }
        }
        
        if (telemetry_ack_received) {
            printf("ACK recebido do servidor (Telemetria)\n");
        }
        telemetry_ack_received = 0; 
        pthread_mutex_unlock(&mutex_telemetry_ack);
    }
    return NULL;
}

//--- Thread 4: simulacao de drones ----
void *thread_drone_sim(void *arg) {
    printf("[Thread Simulacao Drones] Iniciada\n");

    while (1) {
        Mission my_mission;

        pthread_mutex_lock(&mutex_mission);
        while (current_mission.active == 0) {
            pthread_cond_wait(&cond_mission_start, &mutex_mission);
        }
        my_mission = current_mission;
        pthread_mutex_unlock(&mutex_mission);

        printf("\n[MISSAO EM ANDAMENTO]\n");
        printf("Equipe %s atuando em %s\n", 
               amazonia_map.nodes[my_mission.team_id].name, 
               amazonia_map.nodes[my_mission.city_id].name);
        
        int duration = (rand() % 15) + 5; 
        printf("Tempo estimado: %d segundos\n", duration);
        sleep(duration);

        printf("Missao concluida!\n");

        header_t header;
        payload_conclusao_t payload;

        header.type = htons(MSG_CONCLUSAO);
        header.length = htons(sizeof(payload_conclusao_t));
        payload.id_cidade = htonl(my_mission.city_id);
        payload.id_equipe = htonl(my_mission.team_id);

        char buffer[sizeof(header_t) + sizeof(payload_conclusao_t)];
        memcpy(buffer, &header, sizeof(header_t));
        memcpy(buffer + sizeof(header_t), &payload, sizeof(payload_conclusao_t));

        send_udp_packet(buffer, sizeof(buffer));
        printf("Conclusao enviada ao servidor\n");

        pthread_mutex_lock(&mutex_conclusao_ack);
        while (conclusao_ack_received == 0) {
            pthread_cond_wait(&cond_conclusao_ack, &mutex_conclusao_ack);
        }
        conclusao_ack_received = 0;
        pthread_mutex_unlock(&mutex_conclusao_ack);

        pthread_mutex_lock(&mutex_mission);
        current_mission.active = 0;
        pthread_mutex_unlock(&mutex_mission);
    }
    return NULL;
}

// --- Thread 3: udp dispatcher ----
void *thread_receiver(void *arg) {
    printf("[Thread Recepcao] Iniciada\n");
    char buffer[BUF_SIZE];
    struct sockaddr_storage src_addr;
    socklen_t src_len = sizeof(src_addr);

    while (1) {
        ssize_t len = recvfrom(sockfd, buffer, BUF_SIZE, 0, (struct sockaddr *)&src_addr, &src_len);
        if (len < sizeof(header_t)) continue;

        header_t *header = (header_t *)buffer;
        uint16_t type = ntohs(header->type);

        switch (type) {
            case MSG_ACK: {
                payload_ack_t *ack = (payload_ack_t *)(buffer + sizeof(header_t));
                int status = ntohl(ack->status);
                
                if (status == ACK_TELEMETRIA) {
                    pthread_mutex_lock(&mutex_telemetry_ack);
                    telemetry_ack_received = 1;
                    pthread_cond_signal(&cond_telemetry_ack);
                    pthread_mutex_unlock(&mutex_telemetry_ack);
                } else if (status == ACK_CONCLUSAO) {
                    pthread_mutex_lock(&mutex_conclusao_ack);
                    conclusao_ack_received = 1;
                    pthread_cond_signal(&cond_conclusao_ack);
                    pthread_mutex_unlock(&mutex_conclusao_ack);
                }
                break;
            }

            case MSG_EQUIPE_DRONE: {
                payload_equipe_drone_t *order = (payload_equipe_drone_t *)(buffer + sizeof(header_t));
                int city_id = ntohl(order->id_cidade);
                int team_id = ntohl(order->id_equipe);

                printf("\n[ORDEM DE DRONE RECEBIDA]\n");
                printf("Cidade: %s (ID=%d)\n", amazonia_map.nodes[city_id].name, city_id);
                printf("Equipe: %s (ID=%d)\n", amazonia_map.nodes[team_id].name, team_id);

                header_t ack_hdr;
                payload_ack_t ack_pl;
                ack_hdr.type = htons(MSG_ACK);
                ack_hdr.length = htons(sizeof(payload_ack_t));
                ack_pl.status = htonl(ACK_EQUIPE_DRONE);
                
                char ack_buf[sizeof(header_t) + sizeof(payload_ack_t)];
                memcpy(ack_buf, &ack_hdr, sizeof(header_t));
                memcpy(ack_buf + sizeof(header_t), &ack_pl, sizeof(payload_ack_t));
                
                send_udp_packet(ack_buf, sizeof(ack_buf));
                printf("ACK enviado ao servidor\n");

                pthread_mutex_lock(&mutex_mission);
                if (current_mission.active) {
                    printf("AVISO: Ja existe missao ativa, ordem ignorada ou na fila (simulacao simples)\n");
                } else {
                    current_mission.city_id = city_id;
                    current_mission.team_id = team_id;
                    current_mission.active = 1;
                    printf("> Missao registrada para execucao\n");
                    pthread_cond_signal(&cond_mission_start);
                }
                pthread_mutex_unlock(&mutex_mission);
                break;
            }
        }
    }
    return NULL;
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <ipv4|ipv6> [hostname]\n", argv[0]);
        return 1;
    }

    const char *protocol = argv[1];
    const char *hostname = (argc > 2) ? argv[2] : "127.0.0.1";

    if (load_graph("grafo_amazonia_legal.txt", &amazonia_map) != 0) {
        fprintf(stderr, "Erro ao carregar grafo.\n");
        return 1;
    }

    memset(current_status, 0, sizeof(current_status));

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    
    if (strcmp(protocol, "v6") == 0) {
        hints.ai_family = AF_INET6;
        if (argc < 3) hostname = "::1";
    } else {
        hints.ai_family = AF_INET;
    }

    if (getaddrinfo(hostname, PORT, &hints, &res) != 0) {
        perror("getaddrinfo");
        return 1;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    memcpy(&server_addr, res->ai_addr, res->ai_addrlen);
    server_addr_len = res->ai_addrlen;
    freeaddrinfo(res);

    printf("Conectado ao servidor %s:%s\n", hostname, PORT);

    pthread_t t1, t2, t3, t4;

    pthread_create(&t1, NULL, thread_monitoring, NULL);
    pthread_create(&t2, NULL, thread_telemetry, NULL);
    pthread_create(&t3, NULL, thread_receiver, NULL);
    pthread_create(&t4, NULL, thread_drone_sim, NULL);

    printf("Todas as threads iniciadas. Pressione Ctrl+C para encerrar.\n");

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    pthread_join(t4, NULL);

    close(sockfd);
    return 0;
}