#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define PORT "8080"
#define BUF_SIZE 2048

#define MSG_TELEMETRIA 1
#define MSG_ACK 2
#define MSG_EQUIPE_DRONE 3
#define MSG_CONCLUSAO 4

#define ACK_TELEMETRIA 0
#define ACK_EQUIPE_DRONE 1
#define ACK_CONCLUSAO 2

#define MAX_CITIES 50

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t length;
} header_t;

typedef struct __attribute__((packed)) {
    int id_cidade;
    int status;
} telemetria_t;

typedef struct __attribute__((packed)) {
    int total;
    telemetria_t dados[MAX_CITIES];
} payload_telemetria_t;

typedef struct __attribute__((packed)) {
    int status;
} payload_ack_t;

typedef struct __attribute__((packed)) {
    int id_cidade;
    int id_equipe;
} payload_equipe_drone_t;

typedef struct __attribute__((packed)) {
    int id_cidade;
    int id_equipe;
} payload_conclusao_t;

#endif