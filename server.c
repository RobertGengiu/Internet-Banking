#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "util.h"

int main(int argc, char *argv[]) {
  int listen_fd;                      // socket-ul serverului TCP
  int conn_fd;                        // socket-ul clientului TCP
  int sock_udp;                       // socket UDP
  struct sockaddr_in server_tcp_addr; // adresa TCP a serverului
  struct sockaddr_in client_tcp_addr; // adresa TCP a clientului
  struct sockaddr_in server_udp_addr; // adresa UDP a serverului
  struct sockaddr_in client_udp_addr; // adresa UDP a clientului
  fd_set read_fds;                    // multimea folosita la apelul select
  fd_set tmp_fds;
  int fdmax;                          // numar maxim de descriptori
  int i, j;
  char holder[Max];                   // folosit la citire pe post de buffer
  FILE* fp;
  int number;                         // numarul de clienti din user_data_file
  int check_close;  // folosit la apelul recv() pentu a vedea daca s-a inchis
  // sau a dat eroare

  // Numar gresit de parametrii
  if(argc != 3) {
    printf("%d : Eroare la apelul executabilului\n", Wrong_Command);
    return -1;
  }

  // Ne asiguram ca cele doua multimi sunt vide
  FD_ZERO(&read_fds);
  FD_ZERO(&tmp_fds);

  fp = fopen(argv[2], "rt");
  fscanf(fp, "%d", &number);
  if (fp < 0) {
    printf("%d : Eroare la deschidere fisier user_data_file\n", Wrong_Command);
    return -1;
  }

  Client **clients = read_from_file(fp, number);
  if (clients == NULL & number > 0) {
    printf("%d : Eroare la alocarea vectorului de clienti\n", Wrong_Command);
    return -1;
  }

  // Deschidem socket-ul serverului
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    printf("%d : Eroare la apel socket() - TCP\n", Wrong_Command);
    delete_clients(clients, number);
    return -1;
  }

  // Completam informatiile adresei serverului TCP
  memset(&server_tcp_addr, 0, sizeof(server_tcp_addr));
  server_tcp_addr.sin_family = AF_INET;
  server_tcp_addr.sin_addr.s_addr = INADDR_ANY;
  server_tcp_addr.sin_port = htons(atoi(argv[1]));

  // Aplicam proprietatile socketului
  if (bind(listen_fd, (struct sockaddr *)&server_tcp_addr,
  sizeof(server_tcp_addr)) < 0) {
    printf("%d : Eroare la apel bind() - TCP\n", Wrong_Command);
    delete_clients(clients, number);
    return -1;
  }

  // Pornim ascultarea
  listen(listen_fd, Max_Clients);

  // Adaugam socket-ul la multimea read_fds
  FD_SET(listen_fd, &read_fds);
  // Pentru stdin
  FD_SET(0, &read_fds);
  fdmax = listen_fd;

  unsigned int client_udp_lenght = sizeof(client_udp_addr);

  sock_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock_udp < 0) {
    printf("%d : Eroare la apel socket() - UDP\n", Wrong_Command);
    close(listen_fd);
    delete_clients(clients, number);
    return -1;
  }

  // Completam informatiile despre adresa serverului UDP
  memset(&server_udp_addr, 0, sizeof(server_udp_addr));
  server_udp_addr.sin_family = AF_INET;
  server_udp_addr.sin_port = htons(atoi(argv[1]));
  server_udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sock_udp, (struct sockaddr *)&server_udp_addr,
  sizeof(server_udp_addr)) < 0) {
    printf("%d : Eroare la apel bind() - UDP\n", Wrong_Command);
    close(listen_fd);
    delete_clients(clients, number);
    return -1;
  }

  // Daca sock_udp este maximul din read_fds
  if (fdmax < sock_udp) {
    fdmax = sock_udp;
  }

  // Adaugam socket-ul UDP
  FD_SET(sock_udp, &read_fds);

  // Construim lista de clienti activi pentru verificarea sesiunii
  List *lista_clienti_activi = (List *) malloc(sizeof(List));
  if (lista_clienti_activi == NULL) {
    printf("%d : Eroare la alocarea listei de clienti activi\n", Wrong_Command);
    delete_clients(clients, number);
    close(listen_fd);
    close(sock_udp);
    return -1;
  }
  lista_clienti_activi->head = NULL;
  lista_clienti_activi->number = 0;

  // Construim lista de transferuri
  Transfer_List *transfer = (Transfer_List *) malloc(sizeof(Transfer_List));
  transfer->head = NULL;
  transfer->number = 0;

  for (;;) {
    tmp_fds = read_fds;

    // Multiplexare
    if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) < 0) {
      printf("%d : Eroare la apel select()\n", Wrong_Command);
      delete_clients(clients, number);
      close(listen_fd);
      close(sock_udp);
      return -1;
    }

    // Parcurgem toti descriptori
    for(i = 0; i < fdmax + 1; i++) {
      // Verificam daca e folosit
      if (FD_ISSET(i, &tmp_fds)) {
        // Daca e stdin
        if (i == 0) {
          // Golim holder-ul
          memset(holder, 0, Max);
          // Citim de la tastatura
          fgets(holder, Max-1, stdin);
          int length = strlen(holder);
          // Inlocuim newline cu terminator
          if (holder[length - 1] == '\n') {
            holder[length - 1] = '\0';
          }
          // Daca primim comanda quit trimitem comanda de inchidere catre toti
          // clienti, inchidem socketii si eliberam memoria
          if (strncmp(holder, "quit", 4) == 0) {
            printf("Server-ul se inchide\n");
            for (j = 4; j < fdmax + 1; j++) {
              if (j != sock_udp) {
                // Trimitem mesajul de inchidere
                send(j, holder, Max, 0);
                FD_CLR(j, &read_fds);
              } else {
                close(sock_udp);
                FD_CLR(sock_udp, &read_fds);
              }
            }
            close(listen_fd);
            delete_clients(clients, number);
            delete_list(lista_clienti_activi);
            delete_transfer(transfer);
            close(listen_fd);
            close(sock_udp);
            return 0;
          }
        }
        // Daca e o conexiune UDP
        if (i == sock_udp) {
          // Golim holder-ul
          memset(holder, 0, Max);

          recvfrom(sock_udp, holder, Max, 0, (struct sockaddr*)&client_udp_addr,
          &client_udp_lenght);
          if (strlen(holder) > 1) {
            printf ("Am primit de la clientul de pe socketul UDP, mesajul: %s\n",
            holder);
            char *pch;

            // Cererea de deblocare
            if (strncmp(holder, "unlock", 6) == 0) {
              pch = strtok(holder, " ");
              pch = strtok(NULL, " ");
              char card[7];
              strcpy(card, pch);
              Client *client = get_information(clients, atoi(card), number);
              if (client == NULL) {
                sendto(sock_udp, "wrong_card", 10, 0, (struct sockaddr*)
                &client_udp_addr, sizeof(client_udp_addr));
                continue;
              } else {
                if (client->status == Unlocked) {
                  sendto(sock_udp, "op_fail", 7, 0, (struct sockaddr*)
                  &client_udp_addr, sizeof(client_udp_addr));
                  continue;
                } else {
                  if(client->attempt_to_unlock == true) {
                    sendto(sock_udp, "unlock_fail", 11, 0, (struct sockaddr*)
                    &client_udp_addr, sizeof(client_udp_addr));
                    continue;
                  } else {
                    client->attempt_to_unlock = true;
                    printf("Cerere parola secreta pentru %d\n", atoi(card));
                    sendto(sock_udp, "insert_pass", 11, 0, (struct sockaddr*)
                    &client_udp_addr, sizeof(client_udp_addr));
                    continue;
                  }
                }
              }
            }

            // Introducerea parolei
            if (strncmp(holder, "pass", 4) == 0) {
              pch = strtok(holder, " ");
              pch = strtok(NULL, " ");
              char card[7];
              strcpy(card, pch);
              char pass[9];
              if (strlen(pch) < 2) {
                sendto(sock_udp, "wrong_pass", 10, 0, (struct sockaddr*)
                &client_udp_addr, sizeof(client_udp_addr));
              } else {
                pch = strtok(NULL, " ");
                strcpy(pass, pch);
                Client *client = get_information(clients, atoi(card), number);
                if (strcmp(pass, client->secret_password) == 0) {
                  client->tries = 0;
                  client->status = Unlocked;
                  client->attempt_to_unlock = false;
                  sendto(sock_udp, "succes", 6, 0, (struct sockaddr*)
                  &client_udp_addr, sizeof(client_udp_addr));
                  continue;
                } else {
                  client->status = Locked;
                  client->attempt_to_unlock = false;
                  sendto(sock_udp, "wrong_pass", 10, 0, (struct sockaddr*)
                  &client_udp_addr, sizeof(client_udp_addr));
                  continue;
                }
              }
            }
          }

        }

        // Daca avem o conexiune noua
        if (i == listen_fd) {
          int client_len = sizeof(client_tcp_addr);

          if ((conn_fd = accept(listen_fd, (struct sockaddr *)&client_tcp_addr,
          &client_len)) < 0) {
            printf("%d : Eroare la apel accept()\n", Wrong_Command);
            delete_list(lista_clienti_activi);
            return -1;
          } else {
            // Adaugam socket-ul clientului la lista
            FD_SET(conn_fd, &read_fds);
            // Verificam daca conn_fd e maximul din read_fds
            if (conn_fd > fdmax) {
              fdmax = conn_fd;
            }
          }
          printf("Conexiune noua de la %s, port %d, socket_client %d\n",
          inet_ntoa(client_tcp_addr.sin_addr), ntohs(client_tcp_addr.sin_port),
          conn_fd);

          // Daca primim de la un socket activ
        } else {
          memset(holder, 0, Max);
          // Daca s-a inchis sau a dat eroare
          if ((check_close = recv(i, holder, sizeof(holder), 0) < 1)) {
            // Daca a dat eroare
            if (check_close < 0) {
              printf("%d : Eroare la apel recv()\n", Wrong_Command);
            }
            close(i);
            // Scoatem din multimea de citire socket-ul
            FD_CLR(i, &read_fds);
          } else {
            printf ("Am primit de la clientul de pe socketul %d, mesajul: %s\n",
            i, holder);
            // Comanda de tip login
            if (strncmp(holder, "login", 5) == 0) {
              char *pch;
              char card[7];
              char pin[5];

              if (strlen(holder) < 7) {
                send(i, "wrong_card", 10, 0);
                continue;
              }

              // Obtinem pin-ul si numarul de card
              pch = strtok(holder, " ");
              pch = strtok (NULL, " ");
              if (strlen(pch) > 6 ) {
                send(i, "wrong_card", 10, 0);
                continue;
              }
              strcpy(card, pch);

              if (strlen(pch) < 4) {
                send(i, "wrong_pin", 9, 0);
                continue;
              }

              pch = strtok (NULL, " ");
              strcpy(pin, pch);

              // daca card-ul era deja activ
              if (checkActive(lista_clienti_activi, atoi(card)) == true) {
                send(i, "session_active", 14, 0);
              } else {
                Client *client = get_information(clients, atoi(card), number);
                if (client == NULL) {
                  send(i, "wrong_card", 10, 0);
                } else {
                  if (client->tries == 3) {
                    send(i, "locked", 6, 0);
                  } else {
                    if (client->pin != atoi(pin)) {
                      client->tries += 1;
                      if (client->tries == 3) {
                        send(i, "locked", 6, 0);
                        client->status = Locked;
                      } else {
                        send(i, "wrong_pin", 9, 0);
                      }
                    } else {
                      client->tries = 0;
                      new_active_client(lista_clienti_activi, atoi(card), i);
                      printf("Client nou logat cu numarul %d pe socket-ul %d\n",
                      atoi(card), i);
                      char aux[50];
                      strcpy(aux, "login_accepted ");
                      strcat(aux, client->last_name);
                      strcat(aux, " ");
                      strcat(aux, client->first_name);
                      send(i, aux, strlen(aux), 0);
                    }
                  }
                }
              }
            }

            // Comanda de tip logout
            if (strncmp(holder, "logout", 6) == 0) {
              int check = remove_active_client(lista_clienti_activi, i);
              if (check < 0) {
                send(i, "logout_failed", 13, 0);
              } else {
                send(i, "logout_accepted", 15, 0);
                printf("Client deconectat de pe socket-ul %d\n", i);
              }

            }

            // Comanda de tip listsold
            if (strncmp(holder, "listsold", 8) == 0) {
              int card = get_card(lista_clienti_activi, i);
              //printf("%d\n",card);
              Client *client = get_information(clients, card, number);
              char aux[30];
              char amount[20];
              memset(aux, 30, 0);
              memset(amount, 20, 0);
              sprintf(amount, "%.2lf", client->amount);
              strcpy (aux, "sold ");
              strcat(aux, amount);
              send(i, aux, strlen(aux), 0);
              memset(holder, Max, 0);
            }

            // Comanda de tip quit
            if (strncmp(holder, "quit", 4) == 0) {
              remove_active_client(lista_clienti_activi, i);
              printf("Conexiune inchisa pe socket-ul %d\n", i);
            }

            // Comanda de tip transfer
            if (strncmp(holder, "transfer", 8) == 0) {
              char *pch;
              char card[7];
              char amount[10];
              // Obtinem suma si numarul de card
              pch = strtok(holder, " ");
              pch = strtok(NULL, " ");
              if (strlen(pch) > 6) {
                send(i, "wrong_card", 10, 0);
                continue;
              }
              strncpy(card, pch, 6);

              pch = strtok (NULL, " ");
              strcpy(amount, pch);

              Client *destination = get_information(clients, atoi(card), number);
              if (destination == NULL) {
                send(i, "wrong_card", 10, 0);
              } else {
                Client *source = get_information(clients,
                  get_card(lista_clienti_activi, i), number);
                  if (atof(amount) > source->amount) {
                    send(i, "no_founds", 9, 0);
                  } else {
                    printf("Transfer in curs de pe socket-ul %d\n", i);
                    new_transfer(transfer,i, source->card_number,
                      destination->card_number, atof(amount));
                      char aux[200];
                      strcpy(aux, "confirmation ");
                      strcat(aux, amount);
                      strcat(aux, " ");
                      strcat(aux, destination->last_name);
                      strcat(aux, " ");
                      strcat(aux, destination->first_name);
                      send(i, aux, strlen(aux), 0);
                    }

                  }

                }
                // Procesam transferul
                if (strncmp(holder, "response", 8) == 0) {
                  // Daca am primit confirmarea transferului
                  if(strncmp(holder, "response_y", 10) == 0) {
                    T *t = get_transfer(transfer, i);
                    Client *source = get_information(clients, t->source,
                      number);
                      Client *destination = get_information(clients,
                        t->destination, number);
                        if (t->source != t->destination) {
                          source->amount -= t->amount;
                          destination->amount += t->amount;
                        }
                        send(i, "succes", 6, 0);
                      } else {
                        send(i, "canceled", 8, 0);
                      }

                      remove_transfer(transfer, i);

                    }

                  }
                }
              }
            }
          }

          return 0;
        }
