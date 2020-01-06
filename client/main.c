
#include "client_thread.h"
#include <stdlib.h>
#include <signal.h>

bool connection_opened = true;

int main (int argc, char *argv[argc + 1]) {

    if (argc < 5) {
        fprintf (stderr, "Usage: %s <port-nb> <nb-clients> <nb-requests> <resources>...\n",
                 argv[0]);
        exit (1);
    }

    port_number = atoi (argv[1]);
    //// pour un nombre des clients fixe(5).
    //// Pour l'activer vous le connectez avec init_server
    //// en dÉconnectant init_server_rng, num_client(ligne 53, 54)
    ////  et les srand, sprintf(ligne 170, 171) au lieu de (173)
    ////  au client_thread.c
//    int num_clients = atoi (argv[2]);
    num_request_per_client = atoi (argv[3]);
    num_resources = argc - 4;

    provisioned_resources = malloc (num_resources * sizeof (int));
    for (unsigned int i = 0; i < num_resources; i++)
        provisioned_resources[i] = atoi (argv[i + 4]);

//    init_client(num_clients);

    //// Pour un nombre aléatoire de clients(RNG).
  int num_clients = init_client_rng();

    client_thread *client_threads
            = malloc (num_clients * sizeof (client_thread));
    for (unsigned int i = 0; i < num_clients; i++)
        ct_init (&(client_threads[i]));

    for (unsigned int i = 0; i < num_clients; i++) {
        if(connection_opened)
        { ct_create_and_start(&(client_threads[i])); }
    }

    //// Fpnction responsable à terminer le serveur et libérer
    //// les espaces mémoires capturées durant le programme.
    ct_wait_server (num_clients, client_threads);

    free(client_threads);
    free(provisioned_resources);

    //// Affiche le journal.
    st_print_results (stdout, true);
    FILE *fp = fopen("client.log", "w");
    if (fp == NULL)
    {
        fprintf(stderr, "Could not print log");
        return EXIT_FAILURE;
    }
    st_print_results (fp, false);
    fclose(fp);

    return EXIT_SUCCESS;
}

