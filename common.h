#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

// Constants
#define PORT "8080"
#define BUF_SIZE 4096

// Message Types
#define MSG_TELEMETRIA 1
#define MSG_ACK 2
#define MSG_EQUIPE_DRONE 3
#define MSG_CONCLUSAO 4

// ACK Status Codes
#define ACK_TELEMETRIA 0
#define ACK_EQUIPE_DRONE 1
#define ACK_CONCLUSAO 2

// Fixed size for simplicity as per assignment description (50 max)
#define MAX_CITIES 50

// 1. Header Structure (Shared by all messages)
// Using packed attribute to ensure no padding bytes are inserted by the compiler,
// crucial for sending raw structs over the network.
typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t length; // payload size in bytes
} header_t;

// 2. Telemetry Structures
typedef struct __attribute__((packed)) {
    int id_cidade;
    int status; // 0 = OK, 1 = ALERT
} telemetria_t;

typedef struct __attribute__((packed)) {
    int total; // number of cities monitored
    telemetria_t dados[MAX_CITIES];
} payload_telemetria_t;

// 3. ACK Structure
typedef struct __attribute__((packed)) {
    int status; // Type of ACK
} payload_ack_t;

// 4. Drone Team Order Structure
typedef struct __attribute__((packed)) {
    int id_cidade; // Where the alert is
    int id_equipe; // Which team (capital ID) is dispatched
} payload_equipe_drone_t;

// 5. Conclusion Structure
typedef struct __attribute__((packed)) {
    int id_cidade;
    int id_equipe;
} payload_conclusao_t;

#endif // COMMON_H