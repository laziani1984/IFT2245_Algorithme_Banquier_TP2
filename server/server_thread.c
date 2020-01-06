#define _XOPEN_SOURCE 700   /* So as to allow use of `fdopen` and `getline`.  */

#include "common.h"
#include "server_thread.h"

#include <netinet/in.h>
#include <netdb.h>

#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include <time.h>
#include <semaphore.h>
#include <signal.h>

//// --------------------------------------------------------------------- ////


enum { NUL = '\0' };

enum {
    /* Configuration constants.  */
            max_wait_time = 30,
    server_backlog_size = 5
};

//// --------------------------------------------------------------------- ////

//// ==============+
//// Les variables||
//// ==============+

unsigned int server_socket_fd;

// Nombre de client enregistré.
int nb_registered_clients;

// Variable du journal.
// Nombre de requêtes acceptées immédiatement (ACK envoyé en réponse à REQ).
unsigned int count_accepted = 0;

// Nombre de requêtes acceptées après un délai (ACK après REQ, mais retardé).
unsigned int count_wait = 0;

// Nombre de requête erronées (ERR envoyé en réponse à REQ).
unsigned int count_invalid = 0;


// Nombre de clients qui se sont terminés correctement
// (ACK envoyé en réponse à CLO).
unsigned int count_dispatched = 0;
pthread_mutex_t mutex_count_dispatched = PTHREAD_MUTEX_INITIALIZER;

// Nombre total de requête (REQ) traités.
unsigned int request_processed = 0;

// Nombre de clients ayant envoyé le message CLO.
unsigned int clients_ended = 0;

unsigned int wait_time = 5;
int num_clients = -1;

struct ct_scheduler {
    int *available;
    ct_struct *clients;
    pthread_mutex_t sch_mtx;
} ct_scheduler;

// TODO: Ajouter vos structures de données partagées, ici.
int *max_available, nb_ressources;
sem_t st_sem;

static void sigint_handler(){
    // Code terminaison.
    printf("\nSignal of interruption received!\n"
           "System is preparing for shutdown ... !\n");
    //je n'acccepte plus de connection
    accepting_connections = 0;
}


//// --------------------------------------------------------------------- ////

//// ==============+
//// Les Fonctions||
//// ==============+

// -------------------------------------------------------------------------+
// La fonction responsable à initialiser le serveur et appeler BEGIN et CONF|
// -------------------------------------------------------------------------+

void st_init () {

    //// Initialise le nombre de clients connecté.
    nb_registered_clients = 0;
    // TODO
    //// Établir une connection avec le client pour initialiser le serveur.
    struct sockaddr_in client_address;
    memset(&client_address, 0, sizeof(client_address));
    socklen_t s_length = sizeof(client_address);
    int client_socket = -1, max_time = time(NULL) + max_wait_time;

    signal(SIGINT, &sigint_handler);

    while(client_socket < 0) {
        client_socket = accept(server_socket_fd,
                               (struct sockaddr *) &client_address, &s_length);

        if(time(NULL) >= max_time) {
            perror("Server : Not initialized!");
            exit(-1);
        }
    }

    printf("Server : Server initialized\n");

    ct_scheduler.available = NULL;
    ct_scheduler.clients = NULL;
    pthread_mutex_init(&ct_scheduler.sch_mtx, NULL);
    sem_init(&st_sem, 0, 1);

    initialize_server(client_socket);

    initialize_server(client_socket);

    // END TODO

}

//// --------------------------------------------------------------------- ////

//// ==============+
//// Les Fonctions||
//// ==============+

// --------------------------------+
// Ouvre un socket pour le serveur.|
// --------------------------------+

void st_open_socket (int port_number) {

    server_socket_fd = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (server_socket_fd < 0)
        perror ("Server : ERROR opening socket");

    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEPORT, &(int){ 1 }, sizeof(int)) < 0) {
        perror("Server : setsockopt()");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    memset (&serv_addr, 0, sizeof (serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons (port_number);

    if (bind(server_socket_fd, (struct sockaddr *) &serv_addr,
             sizeof (serv_addr)) < 0)
        perror ("Server : ERROR on binding");

    listen (server_socket_fd, server_backlog_size);

}

//// --------------------------------------------------------------------- ////

//// ===================================================================+
//// La fonction principales qui gère les requêtes à l'aide d'un switch||
//// ===================================================================+

void st_process_requests (server_thread *st, int socket_fd) {

    FILE *socket_read = fdopen(socket_fd, "r");
    FILE *socket_write = fdopen(socket_fd, "w");

    //Gestion user input
    int *parsed_input, current_command;
    char *args, msg[80];
    size_t args_len;

    while (accepting_connections) {

        args_len = 0;
        memset(&msg, 0, sizeof(msg));

        if (socket_read != NULL) {

            args = '\0';

            if (getline(&args, &args_len, socket_read) == -1) {
                if (args) { free(args); }
                break;

            } else {

                printf("Client sent : %s", args);
                parsed_input = parse_input(args);
                current_command = parsed_input[0];
                print_server_data(parsed_input);

                char *tmp;

                switch (current_command) {

                    case 0:
                        treat_begin(socket_fd, current_command, parsed_input);
                        break;
                    case 1:
                        treat_conf(socket_fd, parsed_input);
                        break;
                    case 2:
                        treat_init(parsed_input, st, socket_write);
                        break;

                    case 3:
                        tmp = treat_req(parsed_input, socket_write);
                        if(memcmp(tmp, "5", 1) == 0)
                            { send_wait(socket_write); }
                        break;

                    case 6:
                        treat_end(socket_write);
                        break;

                    case 7:
                        treat_close(parsed_input, st, socket_write);
                        break;

                    default:
                        break;

                }

                free(parsed_input);
                free(args);

            }

        }

    }

    fclose(socket_read);
    fclose(socket_write);

}

//// --------------------------------------------------------------------- ////

//// ========================================+
//// Code main responsable de server threads||
//// ========================================+

void *st_code (void *param) {

    server_thread *st = (server_thread *) param;

    struct sockaddr_in thread_addr;
    socklen_t socket_len = sizeof (thread_addr);
    int thread_socket_fd = -1;
    int end_time = time (NULL) + max_wait_time;

    // Boucle jusqu'à ce que `accept` reçoive la première connection.
    while (thread_socket_fd < 0) {
        thread_socket_fd =
                accept (server_socket_fd, (struct sockaddr *) &thread_addr,
                        &socket_len);

        if (time (NULL) >= end_time) { break; }
    }

    // Boucle de traitement des requêtes.
    while (accepting_connections) {

        if (!nb_registered_clients && time (NULL) >= end_time) {
            fprintf (stderr, "Time out on thread %d.\n", st->id);
            pthread_exit (NULL);
        }

        if (thread_socket_fd > 0) {
            st_process_requests (st, thread_socket_fd);
            close (thread_socket_fd);
            end_time = time (NULL) + max_wait_time;
        }

        thread_socket_fd =
                accept (server_socket_fd, (struct sockaddr *) &thread_addr,
                        &socket_len);
    }

    return NULL;

}

//// --------------------------------------------------------------------- ////

//// ========================================================+
//// Iraitment : Initialisation du serveur et des ressources||
//// ========================================================+

void initialize_server(int client_socket) {

    char *msg, answer[80];
    int *parsed_input, command;
    memset(&msg, 0, sizeof(msg));
    memset(&answer, 0, sizeof(answer));

    //// Receive messsage from client socket.
    recv(client_socket, answer, sizeof(answer), 0);
    if(memcmp(answer, "1", 1) == 0) {
        printf("Client sent : %s", &(answer[0]));
    }

    parsed_input = parse_input(&(answer[0]));
    command = parsed_input[0];
    num_clients = parsed_input[3];

    print_server_data(parsed_input);

    if(command == 0 || command == 1) {
        if(command == 1) { treat_conf(client_socket, parsed_input); }
        else { treat_begin(client_socket, command, parsed_input); }
    } else { send_err(client_socket,
                      "Error in initializing server!", 0); }

    free(parsed_input);

}

        //// +------------------------------------------+ ////



void treat_begin(int socket_fd, int command, int *parsed_input) {

    char msg[64];
    memset(&msg, '\0', sizeof(msg));

    pthread_mutex_lock(&ct_scheduler.sch_mtx);
    // Verify that BEG has not been called already
    if (ct_scheduler.available != NULL) {
        pthread_mutex_unlock(&ct_scheduler.sch_mtx);
        send_err(socket_fd, "Data already sent\n", 0);
    }
    pthread_mutex_unlock(&ct_scheduler.sch_mtx);

    if(parsed_input[1] == 1 && command == 0) {
        sprintf(msg, "4 %d %d\n", parsed_input[1], parsed_input[2]);
    } else { sprintf(msg, "4 0\n"); }
    send_ACK(socket_fd, msg, 0);
}

        //// +------------------------------------------+ ////


void treat_conf(int socket_fd, int *parsed_input) {

    pthread_mutex_lock(&ct_scheduler.sch_mtx);
    if(!is_empty(ct_scheduler.available)) {
        pthread_mutex_unlock(&ct_scheduler.sch_mtx);
        send_err(socket_fd, "Data already sent!", 0);
    }
    pthread_mutex_unlock(&ct_scheduler.sch_mtx);

    char msg[64];
    memset(&msg, '\0', sizeof(msg));
    //// Initialise l'identité du client connecté.
    nb_ressources = parsed_input[1];

    //// Initialise les ressources disponibles, max_disponibles,
    /// allouées et max.
    ct_scheduler.available = malloc(nb_ressources * sizeof(int));

    //// (i + 2) indique la position de res(0), res(1), .., res(n).
    for(int i = 0; i < nb_ressources; i++)
    { ct_scheduler.available[i] = parsed_input[i + 2]; }

    if(is_empty(ct_scheduler.available)) {
        send_err(socket_fd, "Invalid values in conf array!", 0);
    }


    printf("Server : Ressources received\n");
    sprintf(msg, "4 0\n");
    send_ACK(socket_fd, msg, 0);

}


//// --------------------------------------------------------------------- ////

//// =======================================+
//// Iraitment : Initialisation des threads||
//// =======================================+

void treat_init(int *parsed_input,server_thread *st, FILE *socket_write) {

    char msg[64];
    memset(&msg, '\0', sizeof(msg));
    int ct_id = parsed_input[2];

    pthread_mutex_lock(&ct_scheduler.sch_mtx);
    if(is_empty(ct_scheduler.available)) {
        pthread_mutex_unlock(&ct_scheduler.sch_mtx);
        sprintf(msg, "Sequence is not repected!\nINIT comes before CONF\n");
        send_err(0, msg, socket_write);
        return;
    }
    pthread_mutex_unlock(&ct_scheduler.sch_mtx);

    pthread_mutex_lock(&ct_scheduler.sch_mtx);
    if (find_clients(ct_id)) {
        pthread_mutex_unlock(&ct_scheduler.sch_mtx);
        sprintf(msg, "Client already exists\n");
        send_err(0, msg, socket_write);
        return;
    }
    pthread_mutex_unlock(&ct_scheduler.sch_mtx);

    //// Vérifie les ressources max.
    //// Début du semaphore.
    sem_wait(&st_sem);
    for (int i = 0; i < nb_ressources; i++) {
        if (ct_scheduler.available[i] < parsed_input[i + 3]) {
            strcat(msg, "Requested is more than available!\n");
            send_err(0, msg, socket_write);
            sem_post(&st_sem);
            return;
        }
    }

    printf("initializing C!\n");
    //// Initialise les données du client.
    ct_struct *c = malloc(sizeof(ct_struct));
    c->id = parsed_input[2];
    c->closed = false;
    c->waiting = false;
    c->max = malloc(nb_ressources * sizeof(int));
    c->alloc = calloc((unsigned)nb_ressources, sizeof(int));
    c->need = malloc(nb_ressources * sizeof(int));
    nb_registered_clients += 1;

    for (int i = 0; i < nb_ressources
            ; i++) {
        c->need[i] = parsed_input[i + 3];
        c->max[i] = parsed_input[i + 3];
    }

    //// Fin du semaphore.
    sem_post(&st_sem);

    pthread_mutex_lock(&ct_scheduler.sch_mtx);
    //// next sera pointeur vers ct.scheduler.clients
    c->next = ct_scheduler.clients;
    //// Ce client sera le client actuel déjà intialisé
    ct_scheduler.clients = c;
    pthread_mutex_unlock(&ct_scheduler.sch_mtx);

    //// Accepte la connexion.
    printf("Server %d: Connection initialized.\n", st->id);
    sprintf(msg, "4 0\n");
    send_ACK(0, msg, socket_write);

}

//// --------------------------------------------------------------------- ////



// +---------------------------------------+
//  Iraitment : Initialisation des threads |
// +---------------------------------------+

char *treat_req(int *parsed_input, FILE *socket_write) {

    char msg[80];
    memset(&msg, 0, sizeof(msg));
    int ct_id = parsed_input[2];

    sem_wait(&st_sem);
    request_processed += 1;
    sem_post(&st_sem);

    ct_struct *c;

    pthread_mutex_lock(&ct_scheduler.sch_mtx);

    if(!(c = find_clients(ct_id))) {
        pthread_mutex_unlock(&ct_scheduler.sch_mtx);
        sprintf(msg, "Cannot request before initiate\n");
        sem_wait(&st_sem);
        count_invalid += 1;
        sem_post(&st_sem);
        send_err(0, msg, socket_write);
        return "";
    }

    if (c->closed) {
        pthread_mutex_unlock(&ct_scheduler.sch_mtx);
        sem_wait(&st_sem);
        count_invalid++;
        sem_post(&st_sem);
        sprintf(msg, "Cannot REQ after CLO\n");
        send_err(0, msg, socket_write);
        return "";
    }

    pthread_mutex_unlock(&ct_scheduler.sch_mtx);

    int parsed_args[nb_ressources];
    for (int i = 0; i < nb_ressources; i++) { parsed_args[i] = parsed_input[i + 3]; }


    pthread_mutex_lock(&ct_scheduler.sch_mtx);

    if (requested_more_needed(parsed_args, c->need)) {
        pthread_mutex_unlock(&ct_scheduler.sch_mtx);
        sem_wait(&st_sem);
        count_invalid += 1;
        sem_post(&st_sem);
        sprintf(msg, "Requested is more than needed!\n");
        send_err(0, msg, socket_write);
        return "";
    }

    for (int i = 0; i < nb_ressources; i++) {
        if (parsed_args[i] + c->alloc[i] < 0) {
            printf("parsed_args[i] + c->alloc[i] : %d + %d = %d\n\n\n",
                   parsed_args[i], c->alloc[i], parsed_args[i] + c->alloc[i]);
            pthread_mutex_unlock(&ct_scheduler.sch_mtx);
            sem_wait(&st_sem);
            count_invalid += 1;
            sem_post(&st_sem);
            send_err(0, "Free is more than allocated!\n", socket_write);
            return "";
        }
    }


    if (requested_more_needed(parsed_args, ct_scheduler.available)) {

        pthread_mutex_unlock(&ct_scheduler.sch_mtx);
        c->waiting = true;
        sem_wait(&st_sem);
        count_wait += 1;
        sem_post(&st_sem);
        return "5";
    }

    handle_req(parsed_args, ct_scheduler.available, c->alloc, c->need, 0);
    if (!safe_state(nb_registered_clients, ct_scheduler.available,
                    ct_scheduler.clients)) {
        pthread_mutex_unlock(&ct_scheduler.sch_mtx);
        handle_req(parsed_args, ct_scheduler.available, c->alloc, c->need, 1);
        c->waiting = true;
        sem_wait(&st_sem);
        count_wait += 1;
        sem_post(&st_sem);
        return "5";
    }

    pthread_mutex_unlock(&ct_scheduler.sch_mtx);

    if (c->waiting) {
        c->waiting = false;
    }

    sem_wait(&st_sem);
    count_accepted += 1;
    sem_post(&st_sem);
    sprintf(msg, "4 0\n");
    send_ACK(0, msg, socket_write);
    return "";
}

//// --------------------------------------------------------------------- ////

// +----------------------------------+
//  Iraitment : Fermeture des threads |
// +----------------------------------+

void treat_close(int *parsed_input, server_thread *st, FILE *socket_write) {

    char msg[64];
    memset(&msg, '\0', sizeof(msg));
    int ct_id = parsed_input[2];

    ct_struct *c;
    pthread_mutex_lock(&ct_scheduler.sch_mtx);
    if (!(c = find_clients(ct_id))) {
        pthread_mutex_unlock(&ct_scheduler.sch_mtx);
        send_err(0, "Cannot close before initiate\n",  socket_write);
        return;
    }

    if (!is_empty(c->alloc)) {
        pthread_mutex_unlock(&ct_scheduler.sch_mtx);
        send_err(0, "Ressources are not freed\n", socket_write);
        return;
    }
    c->closed = true;
    pthread_mutex_unlock(&ct_scheduler.sch_mtx);

    //// Connexion fermé avec succès.
    printf("Connection between Server(%d) and client(%d) is now closing!\n",
           st->id,c->id);
    printf("dispatching\n");
    pthread_mutex_lock(&mutex_count_dispatched);
    count_dispatched++;
    pthread_mutex_unlock(&mutex_count_dispatched);
    sprintf(msg, "4 0\n");
    send_ACK(0, msg, socket_write);

}

//// --------------------------------------------------------------------- ////

//// +================================+
//// Iraitment : Fermeture du serveur||
//// +================================+

void treat_end(FILE *socket_write) {

    char msg[64];
    memset(&msg, '\0', sizeof(msg));
    accepting_connections = false;
    sem_wait(&st_sem);

    if (count_dispatched < nb_registered_clients) {
        sem_post(&st_sem);
        strcat(msg, "There's still connected clients!\n"
                    "Server is shutting down!\n");
        send_err(0, msg, socket_write);
        return;
    }

    sem_post(&st_sem);
    printf("Server : Closed successfully.\n");
    sprintf(msg, "4 0\n");
    send_ACK(0, msg, socket_write);

}

//// --------------------------------------------------------------------- ////

//// +=========================+
//// Vérificateurs des erreurs||
//// +=========================+


// Source: geeksforgeeks.org/program-bankers-algorithm-set-1-safety-algorithm.
//// Algo de banquier.
bool safe_state(int num_clients, int *available, ct_struct *clients) {

    //// Un tableau initialisé à false pour tous les clients.
    bool finish[num_clients];
    memset(finish, false, num_clients * sizeof(bool));
    ////  .
    int work[nb_ressources];
    memcpy(work, available, nb_ressources * sizeof(int));
    int counter = num_clients;

    while (counter) {

        bool safe = false;
        ct_struct *c = clients;
        for (int i = 0; i < num_clients; i++) {
            if (!finish[i]) {
                int j;
                for (j = 0; j < nb_ressources; j++)
                    if (c->need[j] > work[j])
                        break;

                if (j == nb_ressources) {
                    for (int k = 0; k < nb_ressources; k++)
                        work[k] += c->alloc[k];

                    counter--;
                    finish[i] = true;
                    safe = true;
                }
            }

            c = c->next;
        }

        if (!safe) { return false; }
    }

    return true;
}


//// --------------------------------------------------------------------- ////

//// Vérifie si la liste de ressources contient des éléments nuls ou pas.
bool is_empty(int *ressource) {

    for (int i = 0; i < nb_ressources; i++) {
        if (ressource[i] != 0) { return false; }
    }
    return true;

}

//// REQ > ressources allouées.
bool requested_more_needed(int *res_arr1, int *res_arr2) {

    for (int i = 0; i < nb_ressources; i++) {
        if (res_arr1[i] > res_arr2[i]) { return true; }
    }
    return false;

}

//// Vérifiie si le client id est déjà dans la liste
ct_struct *find_clients(int id) {

    ct_struct *c = ct_scheduler.clients;
    while(c != NULL) {
        if(c->id == id) { return c; }
        c = c->next;
    }
    return NULL;
}

//// --------------------------------------------------------------------- ////

//// +================================================+
////  Allouer et libérer les ressources de la requête||
//// +================================================+

void handle_req(int *requested, int *available, int *allocated,
        int *needed, int will_allocate) {

    for (int i = 0; i < nb_ressources; i++) {

        //// Allouer des ressources.
        if(will_allocate == 0) {
            available[i] -= requested[i];
            allocated[i] += requested[i];
            needed[i] -= requested[i];
            //// Libérer les ressources.
        } else {
            available[i] += requested[i];
            allocated[i] -= requested[i];
            needed[i] += requested[i];
        }

    }

}

//// --------------------------------------------------------------------- ////

//// +=================================+
////  Iraitment des messages à envoyer||
//// +=================================+

//// ACK
void send_ACK(int ct_socket, char *msg, FILE *socket_write) {

    if (socket_write != 0) {
        fprintf(socket_write, msg, strlen(msg), 0);
        fflush(socket_write);
    } else { send(ct_socket, msg, strlen(msg), 0); }

}


        //// +------------------------------------------+ ////

//// ERR
void send_err(int ct_socket, char *msg, FILE *socket_write) {

    char send_msg[64];
    memset(&send_msg, '\0', sizeof(send_msg));
    sprintf(send_msg, "8 %ld %s\n", strlen(msg), msg);
    if (socket_write != 0) {
        fprintf(socket_write, msg, strlen(msg), 0);
        fflush(socket_write);
    } else { send(ct_socket, msg, strlen(msg), 0); }

}

        //// +------------------------------------------+ ////

//// WAIT
void send_wait(FILE *socket_write) {

    char msg[64];
    memset(&msg, '\0', sizeof(msg));
    srand((unsigned)time(NULL));
    sprintf(msg, "5 1 %d\n",(unsigned)(random() % wait_time));
    fprintf(socket_write, msg, strlen(msg), 0);
    fflush(socket_write);

}

//// --------------------------------------------------------------------- ////

//// +===============================================+
////  Fonction responsable à libérer l'espace mémoire|
//// +===============================================+

void st_signal () {

    // TODO: Remplacer le contenu de cette fonction
    free(ct_scheduler.available);
    free_ct_structs(ct_scheduler.clients);
    pthread_mutex_destroy(&ct_scheduler.sch_mtx);
    pthread_mutex_destroy(&mutex_count_dispatched);
    free(max_available);
    sem_destroy(&st_sem);
    // TODO end

}

        //// +------------------------------------------+ ////

void free_ct_structs(ct_struct *head) {

    ct_struct *tmp;
    while (head != NULL) {
        tmp = head;
        head = head->next;
        free(tmp->max);
        free(tmp->alloc);
        free(tmp->need);
        free(tmp);
    }

}

//// --------------------------------------------------------------------- ////

//// ===================+
//// Gestion d'affichage|
//// ===================+

void print_server_data(int *parsed_input) {

    printf("Allocations initialized.\n");
    if(parsed_input[0] == 0) {
        printf("Client sent : %d %d %d\n", parsed_input[0],
               parsed_input[1], parsed_input[2]);
    }

    printf("Commande : %d\n", parsed_input[0]);
    printf("nb_args : %d\n", parsed_input[1]);

    if(parsed_input[1] != 0)
    { printf("Client_id : %d\n", parsed_input[2]); }
    printf("---------------Server---------------\n");

}

//
// Affiche les données recueillies lors de l'exécution du
// serveur.
// La branche else ne doit PAS être modifiée.
//
void st_print_results (FILE * fd, bool verbose) {

    if (fd == NULL) fd = stdout;

    if (verbose) {
        fprintf (fd, "\n---- Résultat du serveur ----\n");
        fprintf (fd, "Requêtes acceptées: %d\n", count_accepted);
        fprintf (fd, "Requêtes en attentes : %d\n", count_wait);
        fprintf (fd, "Requêtes invalides: %d\n", count_invalid);
        fprintf (fd, "Clients terminés : %d\n", count_dispatched);
        fprintf (fd, "Requêtes traitées: %d\n", request_processed);
    }
    else
        fprintf (fd, "%d %d %d %d %d\n", count_accepted, count_wait,
                 count_invalid, count_dispatched, request_processed);

}