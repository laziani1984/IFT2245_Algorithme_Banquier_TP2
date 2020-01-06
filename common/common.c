#include <memory.h>
#include "common.h"

/* === Client response === */
cmd_header_t begin_cmd = { .cmd = BEGIN, .nb_args = 1 },
        conf_cmd = { .cmd = CONF }, end_cmd = { .cmd = END, .nb_args = 0 },
        init_cmd = { .cmd = INIT }, req_cmd = { .cmd = REQ },
        clo_cmd = { .cmd = CLO, .nb_args = 1 };

/* === Server response === */
cmd_header_t start_ack_cmd = { .cmd = ACK, .nb_args = 1},
        ack_cmd = { .cmd = ACK, .nb_args = 0}, err_cmd = { .cmd = ERR},
        wait_cmd = { .cmd = WAIT, .nb_args = 1},
        nb_cmnds_cmd = {.cmd = NB_COMMANDS};

ssize_t read_socket(int sockfd, void *buf, size_t obj_sz, int timeout) {
  int ret;
  int len = 0;

  struct pollfd fds[1];
  fds->fd = sockfd;
  fds->events = POLLIN;
  fds->revents = 0;

  do {
    // wait for data or timeout
    ret = poll(fds, 1, timeout);

    if (ret > 0) {
      if (fds->revents & POLLIN) {
        ret = recv(sockfd, (char*)buf + len, obj_sz - len, 0);
        if (ret < 0) {
          // abort connection
          perror("recv()");
          return -1;
        }
        len += ret;
      }
    } else {
      // TCP error or timeout
      if (ret < 0) {
        perror("poll()");
      }
      break;
    }
  } while (ret != 0 && len < obj_sz);
  return ret;
}

/////Retourne un pointeur vers un tableau d'entier.
int *parse_input(char *param) {

  int answer_length = 0, command = 0;
  int *input = malloc(80 * sizeof(int));
  char *ptr = strtok(param, " \n");
  while (ptr != NULL) {
    if (strlen(ptr) != 0) { input[answer_length++] = atoi(ptr); }
    ptr = strtok(NULL, " \n");
  }

  if(input != NULL) {

    command = input[0];
    input[answer_length] = '\0';

    if(command > nb_cmnds_cmd.cmd || command < 0){
      printf("Invalid command!\n");
    } else { return input; }

  }

  free(input);
  return NULL;

}