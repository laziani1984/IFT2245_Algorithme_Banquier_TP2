#ifndef SERVER_THREAD_H
#define SERVER_THREAD_H

#include "common.h"

extern bool accepting_connections;

typedef struct server_thread server_thread;

struct server_thread {
    unsigned int id;
    pthread_t pt_tid;
    pthread_attr_t pt_attr;
};

typedef struct ct_struct {
    int id;
    bool closed;
    bool waiting;
    int *max;
    int *alloc;
    int *need;
    struct ct_struct *next;
} ct_struct;

//// -------------------------------------------------------------- ////
//// Fonctions d'initialisation.
void st_init (void);
void st_open_socket (int port_number);
void initialize_server(int);

//// +------------------------------------------+ ////
//// Fonctions de traitement des commandes.
//void st_create_and_start(st);
void *st_code (void *);
void treat_begin(int, int, int *);
void treat_conf(int, int *);
void treat_init(int *, server_thread *, FILE *);
char *treat_req(int *, FILE *);
void treat_end(FILE *);
void treat_close(int *, server_thread *, FILE *);

//// +------------------------------------------+ ////
//// Gestion de réponses(ACK, ERR, WAIT).
void send_ACK(int, char *, FILE *);
void send_err(int, char *, FILE *);
void send_wait(FILE *);

//// +------------------------------------------+ ////
//// Fonctions de comparaisons.
bool safe_state(int, int*, ct_struct *);
void handle_req(int *, int *, int *, int *, int);
bool requested_more_needed(int *, int *);
bool is_empty(int *);
ct_struct *find_clients(int);

//// +------------------------------------------+ ////
//// Fonctions de traitement des allovations mémoires.
void st_signal (void);
void free_ct_structs(ct_struct *);

//// +------------------------------------------+ ////
//// Fonction d'affichage.
void print_server_data(int *);
void st_print_results (FILE *, bool);

//// -------------------------------------------------------------- ////

#endif

