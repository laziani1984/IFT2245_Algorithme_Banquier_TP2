#ifndef CLIENTTHREAD_H
#define CLIENTTHREAD_H

#include "common.h"

/* Port TCP sur lequel le serveur attend des connections.  */
extern int port_number;

/* Nombre de requêtes que chaque client doit envoyer.  */
extern int num_request_per_client;

/* Nombre de resources différentes.  */
extern int num_resources;

/* Quantité disponible pour chaque resource.  */
extern int *provisioned_resources;

bool connection_opened;


typedef struct client_thread client_thread;
struct client_thread {
    unsigned int id;
    pthread_t pt_tid;
    pthread_attr_t pt_attr;
};

//// -------------------------------------------------------------- ////

//// ==============+
//// Les Fonctions||
//// ==============+

//// Les fonctons qui initialise le client et établit une connection
//// client-serveur pour chacun des clients dans la liste d'ordonnacement.
//// On a deux fonctions init : init_client(utilise le nombre des clients
///  passé en paramètre) et init_client_rng(produit un nombre aléatoire
///  entre 1 et 10 qui sera le nombre de clients).
void init_client(int);
int init_client_rng();
int open_connection();

//// +------------------------------------------+ ////

//// Les fonctions responsables à créer un thread et lui faire commencer
//// son déroulement.
void ct_init (client_thread *);
void ct_create_and_start (client_thread *);

//// +------------------------------------------+ ////

//// Les fonctions envoi-reçu pour se communiquer avec le serveur.
void send_msg(int, cmd_header_t, int, int, client_thread *);
void send_request(int, int, int);
void receive_msg(int, cmd_header_t, int);

//// +------------------------------------------+ ////

//// La fonction responsable à libérer l'espace mémoire retenue durant
//// le déroulement du client.
void ct_wait_server (int, client_thread *);

//// +------------------------------------------+ ////

//// La fonction responsable à afficher les données à la fin du programme.
void print_data(int *);
void st_print_results (FILE *, bool);

//// -------------------------------------------------------------- ////



#endif // CLIENTTHREAD_H


