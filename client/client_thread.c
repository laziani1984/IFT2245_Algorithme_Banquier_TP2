/*
 *  Nom d'auteur : Wael ABOU ALI.
 *  Version : 1.0
 *  Utilisation : C'est un programme simple qui sert à trasmettre les
 *  messages entre un client et un utilisateur en utilisant des sockets.
 *  Le but du programme est de se familiariser avec les concepts de
 *  conditions de course, l'utilisation des mutexes et sémaphores,
 *  l'application des sockets, la gestion manuelle des ressources d'un
 *  programme en utilisant le code banquier du djikstra et se familiariser
 *  avec le fonctionnement d'un ordonnaceur des threads.
 */


/* This `define` tells unistd to define usleep and random.  */
#define _XOPEN_SOURCE 500

#include "common.h"
#include <memory.h>
#include <semaphore.h>
#include <netinet/in.h>
#include "client_thread.h"

//// -------------------------------------------------------------- ////

//// ==============+
//// Les variables||
//// ==============+

int port_number = -1;
int num_request_per_client = -1;
int num_resources = -1;
int *provisioned_resources = NULL;

// Variable d'initialisation des threads clients.
unsigned int count = 0;


// Variable du journal.
// Nombre de requête acceptée (ACK reçus en réponse à REQ)
unsigned int count_accepted = 0;

// Nombre de requête en attente (WAIT reçus en réponse à REQ)
unsigned int count_on_wait = 0;
pthread_mutex_t mutex_count_on_wait = PTHREAD_MUTEX_INITIALIZER;


// Nombre de requête refusée (REFUSE reçus en réponse à REQ)
unsigned int count_invalid = 0;
pthread_mutex_t mutex_count_invalid = PTHREAD_MUTEX_INITIALIZER;

// Nombre de client qui se sont terminés correctement (ACC reçu en réponse à END)
unsigned int count_dispatched = 0;
pthread_mutex_t mutex_count_dispatched = PTHREAD_MUTEX_INITIALIZER;

// Nombre total de requêtes envoyées.
unsigned int request_sent = 0;
pthread_mutex_t mutex_request_sent = PTHREAD_MUTEX_INITIALIZER;


int max_wait = 30, *allocated = NULL, *max = NULL, *ressource, num_clients = -1;
time_t t;

sem_t ct_sem;

//// -------------------------------------------------------------- ////

//// ==============+
//// Les Fonctions||
//// ==============+

// -------------------------------------------------------------------------+
// La fonction responsable à initialiser le client et appeler BEGIN et CONF|
// -------------------------------------------------------------------------+

int init_client_rng() {

    //// Ouvre une connection avec le serveur.
    int socket_fd = open_connection();
    if(socket_fd < 0) exit(-1);

    srand((unsigned)time(&t));
    num_clients = (unsigned)((random() % 10) + 1);
    conf_cmd.nb_args = num_resources, end_cmd.cmd_length = 2,
    init_cmd.nb_args = num_resources + 1, req_cmd.nb_args = num_resources + 1;

    //// Demande la mémoire pour les structures.
    allocated = malloc(num_clients * num_resources * sizeof(int));
    max = malloc(num_clients * num_resources * sizeof(int));
    ressource = malloc(num_clients * num_resources * sizeof(int));

    //// Initialise le sémaphore.
    sem_init(&ct_sem, 0, 1);

    //// Envoyer 0 1 RNG et reçevoir la réponse.
    send_msg(socket_fd, begin_cmd, 0, 0, 0);

    //// Provisionne les ressources au serveur.
    send_msg(socket_fd, conf_cmd, 0, 0, 0);


    //// Ferme la connexion.
    close(socket_fd);

    return num_clients;

}

//// +------------------------------------------+ ////

//// Cette fonction est applicable avec nombre de clients fixe qui sera passé en
//// paramètre ayant le nom num_cts
//void init_client(int num_cts) {
//
//    //// Ouvre une connection avec le serveur.
//    int socket_fd = open_connection();
//    if(socket_fd < 0) exit(-1);
//
//    num_clients = num_cts;
//    conf_cmd.nb_args = num_resources, end_cmd.cmd_length = 2,
//    init_cmd.nb_args = num_resources + 1, req_cmd.nb_args = num_resources + 1;
//
//    //// Demande la mémoire pour les structures.
//    allocated = malloc(num_clients * num_resources * sizeof(int));
//    max = malloc(num_clients * num_resources * sizeof(int));
//    ressource = malloc(num_clients * num_resources * sizeof(int));
//
//    //// Initialise le sémaphore.
//    sem_init(&ct_sem, 0, 1);
//
//    //// Envoyer 0 1 RNG et reçevoir la réponse.
//    send_msg(socket_fd, begin_cmd, 0, 0, 0);
//
//    //// Provisionne les ressources au serveur.
//    send_msg(socket_fd, conf_cmd, 0, 0, 0);
//
//
//    //// Ferme la connexion.
//    close(socket_fd);
//
//}

//// +------------------------------------------+ ////

// --------------------------------+
// Ouvre un socket pour le serveur.|
// --------------------------------+

int open_connection() {

    int server_socket = socket (AF_INET, SOCK_STREAM, 0);

    if(server_socket < 0) {
        perror("Client : In opening socket!");
        return -1;
    }

    struct sockaddr_in server_address;
    memset (&server_address, 0, sizeof (server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons (port_number);

    int max_time = time(NULL) + max_wait;
    while(connect(server_socket, (struct sockaddr *)&server_address,
                  sizeof(server_address))) {
        if(max_time < time(NULL)) {
            perror("Client : In establishing connection!");
            return -1;
        }

        if(!connection_opened) {
            printf("Connection with server disconected!\n");
            return 1;
        }
    }

    return server_socket;

}

//// -------------------------------------------------------------- ////

//// =====================+
//// Fonction main du code|
//// =====================+

// -------------------------------------------------------------------+
// La fonction main du client_thread qui va initialiser les clients et|
// envoyer les requêtes.                                              |
// -------------------------------------------------------------------+

void *ct_code (void *param) {

    int socket_fd = -1;
    client_thread *ct = (client_thread *) param;

    // ouvrir une connection avec le serveur.
    socket_fd = open_connection();
    if(socket_fd < 0) {
        printf("Client %d : Couldn't connect with server!\n", ct->id);
        pthread_exit(NULL);
    }

    printf("Connection established between client %d and server\n", ct->id);
    send_msg(socket_fd, init_cmd, 0, ct->id, ct);

    // TP2 TODO
    // Vous devez ici faire l'initialisation des petits clients (`INI`).
    // TP2 TODO:END

    for (unsigned int request_id = 0; request_id < num_request_per_client;
         request_id++) {

        // TP2 TODO
        // Vous devez ici coder, conjointement avec le corps de send request,
        // le protocole d'envoi de requête.
        if(connection_opened)
        { send_request (ct->id, request_id, socket_fd); }


        // TP2 TODO:END

        /* Attendre un petit peu (0s-0.1s) pour simuler le calcul.  */
        usleep (random () % (100 * 1000));
        /* struct timespec delay;
         * delay.tv_nsec = random () % (100 * 1000000);
         * delay.tv_sec = 0;
         * nanosleep (&delay, NULL); */
    }

    if(connection_opened) { send_msg(socket_fd, clo_cmd, 0, 0, ct); }
    close(socket_fd);
    pthread_exit (NULL);

}

//// +------------------------------------------+ ////

void ct_init (client_thread * ct) { ct->id = count++; }

//// +------------------------------------------+ ////

void ct_create_and_start (client_thread * ct) {

    pthread_attr_init (&(ct->pt_attr));
    //// On le ferme pour que le programme attend les threads pour terminer.
//    pthread_attr_setdetachstate(&(ct->pt_attr), PTHREAD_CREATE_DETACHED);
    pthread_create (&(ct->pt_tid), &(ct->pt_attr), &ct_code, ct);

}

//// -------------------------------------------------------------- ////

//// ======================================================================+
//// Fonctions responsables à envoyer les messages : send_msg, send_request|
//// ======================================================================+

// ----------------------------------------+
// Va envoyer le message assigné au serveur|
// ----------------------------------------+

void send_msg(int socket_fd, cmd_header_t command, int request_id,
              int client_id, client_thread *ct) {

    char cmnd[10], msg[80];
    memset(&msg, 0, sizeof(msg));
    memset(&cmnd, 0, sizeof(cmnd));

    //// Le message sera composé de : (commande) (nb_args) tid.
    if(command.cmd == 2 || command.cmd == 3 || command.cmd == 7)
    { sprintf(cmnd, "%d %d %d", command.cmd, command.nb_args, client_id); }
        //// Le message sera composé de : (commande) (nb_args).
    else { sprintf(cmnd, "%d %d", command.cmd, command.nb_args); }

    strcat(msg, cmnd);

    switch(command.cmd) {

        //// BEGIN(0) 1 RNG.
        case 0:

            //// si c'est un nombre de clients fixe ,donc RNG ne représente
            //// pas le nombre de clients.
            srand((unsigned)time(&t));
            sprintf(cmnd, " %d", (unsigned)(random() % 15) + 1);
            //// Si RNG représente le nombre de clients.
//            sprintf(cmnd, " %d", num_clients);
            strcat(msg, cmnd);
            break;

            //// CONF(1) nb_ressources res(1)  ...  res(n).
        case 1:

            for (int i = 0; i < num_resources; i++) {
                memset(&cmnd, 0, sizeof(cmnd));
                sprintf(cmnd, " %d", provisioned_resources[i]);
                strcat(msg, cmnd);
            }
            break;

            //// INIT(2) nb_ressources+1 tid max_res(1)  ...  max_res(n).
        case 2:

            for (int i = 0; i < num_resources; i++) {
                memset (&cmnd, 0, sizeof(cmnd));
                int curr_res = (int)(random() % (provisioned_resources[i] + 1));
                max[(ct->id * num_resources) + i] = curr_res;
                allocated[(ct->id * num_resources) + i] = 0;
                sprintf(cmnd, " %d", curr_res);
                strcat(msg, cmnd);
            }
            break;

            //// REQ(3) nb_ressources+1 tid max_res(1)  ...  res(n).
        case 3:

            for (int i = 0; i < num_resources; i++) {

                memset(&cmnd, 0, sizeof(cmnd));
                int client_pos = (client_id * num_resources) + i;

                if(request_id == num_request_per_client - 1) {
                    ressource[client_pos] = -1 * allocated[client_pos];
                } else {
                    int rand_max = (unsigned int)(random() % (max[client_pos] + 1));
                    int alloc = allocated[client_pos];
                    ressource[client_pos] = rand_max - alloc;
                }

                sprintf(cmnd, " %d", ressource[client_pos]);
                strcat(msg, cmnd);
            }

            break;

            //// END(6) 0
        case 6:
            //// CLO(7) 1 tid
        case 7:
            break;
            //// Autres instructions.
        default:
            break;

    }

    //// Si c'est BEGIN donc il va envoyer un message composé de 3
    //// arguments (0(commande) 1(nb. arguments) RNG).
    //// Sinon, ça sera le message actuel composé dans le switch.
    if(command.cmd == 0) {
        sprintf(cmnd, " %d\n", num_clients);
        strcat(msg, cmnd);
    } else { strcat(msg, "\n"); }

    //// Il va envoyer le message et attendre la réponse. Si c'est
    //// REQ ou close, il y aura une condition de course pour modifier
    //// les variables et donc nous aurons besoin d'un sémaphore aussi
    //// pour garantir l'accès pour un seul thread à la fois.
    send(socket_fd, msg, strlen(msg), 0);
    if(command.cmd == 3 || command.cmd == 7) { sem_wait(&ct_sem); }
    receive_msg(socket_fd, command, client_id);
    if(command.cmd == 3 || command.cmd == 7) { sem_post(&ct_sem); }

}

        //// +------------------------------------------+ ////

// Vous devez modifier cette fonction pour faire l'envoie des requêtes
// Les ressources demandées par la requête doivent être choisies aléatoirement
// (sans dépasser le maximum pour le client). Elles peuvent être positives
// ou négatives.
// Assurez-vous que la dernière requête d'un client libère toute les ressources
// qu'il a jusqu'alors accumulées.
void send_request(int client_id, int request_id, int socket_fd) {

    // TP2 TODO
    //// Utilisé pour imprimer le numéro de la requête.
    fprintf (stdout, "Client %d is sending its %d request\n", client_id,
             request_id);

    //// Met à jour le nombre total des requêtes.
    sem_wait(&ct_sem);
    request_sent += 1;
    sem_post(&ct_sem);

    send_msg(socket_fd, req_cmd, request_id, client_id, 0);

    // TP2 TODO:END

}

//// -------------------------------------------------------------- ////

//// =======================================================================+
//// Fonctions responsables à recevoir les messages du serveur : receive_msg|
//// =======================================================================+

void receive_msg(int socket_fd, cmd_header_t command, int client_id) {

    char answer[80];
    int *parsed_input, answer_command;
    memset(&answer, 0, sizeof(answer));
    recv(socket_fd, answer, sizeof(answer), 0);

    if(answer[0] != '\0') {

        printf("Server replied : %s", (&answer[0]));
        parsed_input = parse_input(answer);
        answer_command = parsed_input[0];
        print_data(parsed_input);

        switch (answer_command) {

            //// ACK
            case 4:
                switch (command.cmd) {

                    //// Si c'est REQ
                    case 3:
                        for (int i = 0; i < num_resources; i++) {
                            int client_pos = (client_id * num_resources) + i;
                            allocated[client_pos] += ressource[client_pos];
                        }
                        count_accepted += 1;
                        break;

                    //// CLO
                    case 7:

                        count_dispatched += 1;
                        break;

                    default:
                        break;

                }

                break;

            //// WAIT
            case 5:
                //// Ça sera une valeur aléatoire entre 1 et 5.
                sleep(parsed_input[2]);
                pthread_mutex_lock(&mutex_count_on_wait);
                count_on_wait += 1;
                pthread_mutex_unlock(&mutex_count_on_wait);
                //// Pour reappeler le thread qui est en wait.
//                socket_fd = -1;
//                while(socket_fd == -1) {
//                    socket_fd = open_connection();
//                }
//                printf("new_socket : %d\n", socket_fd);
//                send_request(client_id, request_id, socket_fd);
                break;

            //// ERR
            case 8:
            default:
                if (command.cmd == 0 || command.cmd == 1) {
                    free(parsed_input);
                    exit(-1);
                } else { if (command.cmd == 3) { count_invalid += 1; }}
                break;
        }

        free(parsed_input);

    } else { connection_opened = false; }

}

//// -------------------------------------------------------------- ////

//// =====================================================================+
//// Fonction qui prépare à la fermeture en libérant les ressources acquis|
///  durant le programme                                                  |
//// =====================================================================+

//
// Vous devez changer le contenu de cette fonction afin de régler le
// problème de synchronisation de la terminaison.
// Le client doit attendre que le serveur termine le traitement de chacune
// de ses requêtes avant de terminer l'exécution.
//
void ct_wait_server (int num_clients, client_thread *cts) {

    // TP2 TODO

    for(int i = 0; i < num_clients; i++) {
        void *val;
        pthread_join(cts[i].pt_tid, &val);
    }

    free(max);
    free(allocated);
    free(ressource);
    sem_destroy(&ct_sem);
    pthread_mutex_destroy(&mutex_count_on_wait);
    pthread_mutex_destroy(&mutex_count_invalid);

    int socket_fd = open_connection();
    if(socket_fd < 0) exit(-1);
    send_msg(socket_fd, end_cmd, 0, 0, 0);

    // TP2 TODO:END

}

//// -------------------------------------------------------------- ////

//// =============================================+
//// Fonctions responsables à imprimer les données|
//// =============================================+

// Fonction responsable à imprimer les données : commande, nb_args et
// client_id.
void print_data(int *parsed_input) {

    printf("Commande : %d\n", parsed_input[0]);
    printf("nb_args : %d\n", parsed_input[1]);
    if (parsed_input[1] != 0) { printf("Client_id : %d\n", parsed_input[2]); }
    printf("---------------Client---------------\n");

}

        //// +------------------------------------------+ ////

//
// Affiche les données recueillies lors de l'exécution du
// serveur.
// La branche else ne doit PAS être modifiée.
//
void st_print_results (FILE * fd, bool verbose) {

    if (fd == NULL) { fd = stdout; }

    if (verbose) {
        fprintf (fd, "\n---- Résultat du client ----\n");
        fprintf (fd, "Requêtes acceptées: %d\n", count_accepted);
        fprintf (fd, "Requêtes en attentes : %d\n", count_on_wait);
        fprintf (fd, "Requêtes invalides: %d\n", count_invalid);
        fprintf (fd, "Clients terminés : %d\n", count_dispatched);
        fprintf (fd, "Requêtes envoyées: %d\n", request_sent);
    }

    else {
        fprintf (fd, "%d %d %d %d %d\n", count_accepted, count_on_wait,
                 count_invalid, count_dispatched, request_sent);
    }

}

//// -------------------------------------------------------------- ////
