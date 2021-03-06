#include <stdlib.h>

#include "server_thread.h"
#include <pthread.h>
#include <signal.h>

bool accepting_connections = true;

//// Fonction responsable d'effectuer une terminaison prématuré du programme.
static void sigint_handler(){
  // Code terminaison.
  printf("\nSignal of interruption received!\n"
         "System is preparing for shutdown ... !\n");
  //je n'acccepte plus de connection
  accepting_connections = 0;
}

int main (int argc, char *argv[argc + 1]) {

  if (argc < 3) {
    fprintf (stderr, "Usage: %s [port-nb] [nb-threads]\n", argv[0]);
    exit (1);
  }


  int port_number = atoi (argv[1]);
  int num_server_threads = atoi (argv[2]);
  server_thread *st = malloc (num_server_threads * sizeof (server_thread));

  // Ouvre un socket
  st_open_socket (port_number);

  // Initialise le serveur.
  st_init ();

  signal(SIGINT, &sigint_handler);


  // Part les fils d'exécution.
  for (unsigned int i = 0; i < num_server_threads; i++)
  {
    st[i].id = i;
    pthread_attr_init (&(st[i].pt_attr));
    pthread_create (&(st[i].pt_tid), &(st[i].pt_attr), &st_code, &(st[i]));
  }

  for (unsigned int i = 0; i < num_server_threads; i++)
    pthread_join (st[i].pt_tid, NULL);

  // Signale aux clients de se terminer et libère les ressources.
  st_signal();
  free(st);

  // Affiche le journal.
  st_print_results (stdout, true);
  FILE *fp = fopen("server.log", "w");

  if (fp == NULL) {
    fprintf(stderr, "Could not print log");
    return EXIT_FAILURE;
  }

  st_print_results (fp, false);
  fclose(fp);

  return EXIT_SUCCESS;
}

