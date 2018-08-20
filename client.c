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
  int sock_tcp;                           // socket-ul clientului tcp
  int sock_udp;                           // socket-ul clientului udp
  struct sockaddr_in server_tcp_addr;     // adresa serverului tcp
  struct sockaddr_in server_udp_addr;     // adresa serverului udp
  fd_set read_fds;                        // multimea folosita la apelul select
  fd_set tmp_fds;
  unsigned int server_udp_length;
  char holder[Max]; // folosit la citire pe post de buffer
  int check;        // folosit la apelul send() pentu a vedea daca s-a inchis
                    // sau a dat eroare


  // Numar gresit de parametrii
  if(argc != 3) {
    printf("%d : Eroare la apelul executabilului\n", Wrong_Command);
    return -1;
  }

  // Deschidem socket-ul clientului tcp
  sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_tcp < 0){
    printf("%d : Eroare la apel socket() - TCP\n", Wrong_Command);
    return -1;
  }

  // Complem informatiile adresei serverului TCP
  server_tcp_addr.sin_family = AF_INET;
  server_tcp_addr.sin_port = htons(atoi(argv[2]));
  inet_aton(argv[1], &server_tcp_addr.sin_addr);

  // Ne conectam la serverul TCP
  if (connect(sock_tcp, (struct sockaddr *)&server_tcp_addr,
    sizeof(server_tcp_addr)) < 0){
    printf("%d : Eroare la apel connect()\n", Wrong_Command);
    return -1;
  }

  // Ne asiguram ca cele doua multimi sunt vide
  FD_ZERO(&read_fds);
  FD_ZERO(&tmp_fds);

  // Adaugam socket-ul la multimea read_fds
  FD_SET(sock_tcp, &read_fds);
  // Pentru stdin
  FD_SET(0, &read_fds);

  bool logged_in = false;              // daca clientul este logat
  bool transfer_attempt = false;  // daca clientul doreste sa confirme transferul
  bool unlock_attempt = false;

  // Deschidem socket-ul clientului UDP
  sock_udp = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock_udp < 0){
        printf("%d : Eroare la apel socket() - UDP\n", Wrong_Command);
        return -1;
  }

  // Complem informatiile adresei serverului UDP
  server_udp_addr.sin_family = AF_INET;
  server_udp_addr.sin_port = htons(atoi(argv[2]));
  inet_aton(argv[1], &server_udp_addr.sin_addr);

  server_udp_length = sizeof(server_udp_addr);


  int log_fp = -1;
  make_log_file(getpid(), &log_fp);
  if (log_fp < 0) {
    printf("%d : Eroare la creare fisier\n", Wrong_Command);
    return -1;
  }

  int length;
  char last_card[7];
  for(;;) {

    tmp_fds = read_fds;

    // Multiplexare
    select(sock_tcp + 1, &tmp_fds, NULL, NULL, NULL);

    // Verificam daca e de la stdin
    if (FD_ISSET(0, &tmp_fds)) {
      // Golim holder-ul
      memset(holder, 0 , Max);
      // Citim de la tastatura
      fgets(holder, Max - 1, stdin);
      length = strlen(holder);
      // Inlocuim newline cu terminator
      if (holder[length - 1] == '\n') {
        holder[length - 1] = '\0';
      }

      // Scriem comanda in fisier
      write(log_fp, holder, length);
      write(log_fp, "\n", 1);

      // Introduce parola secreta
      if (unlock_attempt == true && strlen(holder) > 1) {
        int length = strlen(holder);
        char aux[50];
        // copiem lungimea parolei secrete
        memset(aux, 0, 40);
        strcpy(aux, "pass ");
        strcat(aux, last_card);
        strcat(aux, " ");
        strncat(aux, holder, strlen(holder));
        char aux2[Max];
        memset(aux2, 0, Max);
        // Trimit mesaj la server
        sendto(sock_udp, aux, strlen(aux), 0, (struct sockaddr *)
          &server_udp_addr, sizeof(server_udp_addr));



        // Primesc mesaj de la server
        recvfrom(sock_udp, aux2, Max, 0, (struct sockaddr*)&server_udp_addr,
          &server_udp_length);

        if (strncmp(aux2, "succes", 6) == 0) {
          printf("UNLOCK> Card deblocat\n");
          write(log_fp, "UNLOCK> Card deblocat\n",22);
        }

        if (strncmp(aux2, "wrong_pass", 10) == 0) {
          printf("UNLOCK> -7 : Deblocare esuata\n");
          write(log_fp, "UNLOCK> -7 : Deblocare esuata\n", 30);
        }

        unlock_attempt = false;
      } else {
        // Comanda de aprobare transfer
        if (transfer_attempt == true) {
          char aux[20];
          strcpy(aux, "response_");
          strncat(aux, holder, 1);
          check = send(sock_tcp, aux, 10, 0);

          if (check < 0) {
            printf("%d : Eroare la apelul send()\n", Wrong_Command);
          }
          transfer_attempt = false;
        }

        // Comanda de tip login
        if (strncmp(holder, "login", 5) == 0) {
          // Daca clientul este deja logat
          // tinem minte ultimul card
          memset(last_card, 0, 7);
          char *pch;
          char aux[50];
          memset(aux, 0, 50);
          strcpy(aux, holder);

          pch = strtok(aux, " ");
          pch = strtok (NULL, " ");
          strcpy(last_card, pch);

          if (logged_in == true) {
            printf("IBANK> %d : Sesiune deja deschisa\n", Session_Active);
            write(log_fp, "IBANK> -2 : Sesiune deja deschisa\n", 35);
          } else {
            length = strlen(holder);
            check = send(sock_tcp, holder, length, 0);
            if (check < 0) {
              printf("%d : Eroare la apelul send()\n", Wrong_Command);
            }
          }
        }

        // Comanda de tip logout
        if (strncmp(holder, "logout", 6) == 0) {
          // Verificam daca e logat
          if (logged_in == false) {
            printf("IBANK> -1 : Clientul nu este autentificat\n");
            write(log_fp, "IBANK> -1 : Clientul nu este autentificat\n", 42);
          } else {
            length = strlen(holder);
            check = send(sock_tcp, holder, length, 0);
            if (check < 0) {
              printf("%d : Eroare la apelul send()\n", Wrong_Command);
            }
          }
        }

        // Comanda de tip listsold
        if (strncmp(holder, "listsold", 8) == 0) {
          if (logged_in == false) {
            printf("IBANK> -1 : Clientul nu este autentificat\n");
            write(log_fp, "IBANK> -1 : Clientul nu este autentificat\n", 42);
          } else {
            length = strlen(holder);
            check = send(sock_tcp, holder, length, 0);
            if (check < 0) {
              printf("%d : Eroare la apelul send()\n", Wrong_Command);
            }
          }
        }

        // Comanda de tip transfer
        if (strncmp(holder, "transfer", 8) == 0) {
          if (logged_in == false) {
            printf("IBANK> -1 : Clientul nu este autentificat\n");
            write(log_fp, "IBANK> -1 : Clientul nu este autentificat\n", 42);
          } else {
            length = strlen(holder);
            check = send(sock_tcp, holder, length, 0);
            if (check < 0) {
              printf("%d : Eroare la apelul send()\n", Wrong_Command);
            }
          }
        }


        // Comanda de tip unlock
        if (strncmp(holder, "unlock", 6) == 0 && unlock_attempt == false) {
          char aux[20];
          char aux2[Max];

          memset(aux, 0, 20);
          memset(aux2, 0, Max);

          strcpy(aux, "unlock ");
          strcat(aux, last_card);

          if (logged_in == false) {
            // Trimit mesaj la server
            sendto(sock_udp, aux, strlen(aux), 0, (struct sockaddr *)
              &server_udp_addr, sizeof(server_udp_addr));

            // Primesc mesaj de la server
            recvfrom(sock_udp, aux2, Max, 0, (struct sockaddr*)&server_udp_addr,
              &server_udp_length);

            if (strncmp(aux2, "wrong_card", 10) == 0) {
              printf("UNLOCK> -4 : Numar card inexistent\n");
              write(log_fp, "UNLOCK> -4 : Numar card inexistent\n", 37);
            }

            if (strncmp(aux2, "insert_pass", 11) == 0) {
              unlock_attempt = true;
              printf("UNLOCK> Trimite parola secreta\n");
              write(log_fp, "UNLOCK> Trimite parola secreta\n", 31);
              continue;
            }

            if (strncmp(aux2, "op_fail", 7) == 0) {
              printf("UNLOCK> -6 : Operatiune esuata\n");
              write(log_fp, "UNLOCK> -6 : Operatiune esuata\n", 31);
              unlock_attempt = false;
              continue;
            }

            if (strncmp(aux2, "unlock_fail", 7) == 0) {
              printf("UNLOCK> -7 : Deblocare esuata\n");
              write(log_fp, "UNLOCK> -7 : Deblocare esuata\n", 30);
            }

          } else {
            printf("UNLOCK> -6 : Operatiune esuata\n");
            write(log_fp, "UNLOCK> -6 : Operatiune esuata\n", 31);
          }
        }

        // Comanda de tip quit
        if (strncmp(holder, "quit", 4) == 0) {
          length = strlen(holder);
          check = send(sock_tcp,holder,length, 0);
          if (check < 0) {
            printf("%d : Eroare la apel send()\n", Wrong_Command);
          }

          write(log_fp, "quit\n", 5);

          close(sock_udp);
          close(sock_tcp);
          close(log_fp);
          return 0;

        }
      }
        // Mesaj de la server
    } else {
      memset(holder, 0 , Max);
      recv(sock_tcp, holder, Max, 0);

      // login cu succes
      if (strncmp(holder, "login_accepted", 14) == 0) {
        char nume[20], prenume[20];
        char line[100];
        char *pch;

        pch = strtok(holder, " ");
        pch = strtok (NULL, " ");
        strcpy(nume, pch);
        pch = strtok (NULL, " ");
        strcpy(prenume, pch);

        strcpy(line, "IBANK> Welcome ");
        strcat(line, nume);
        strcat(line, " ");
        strcat(line, prenume);
        strcat(line, "\n");
        printf("%s", line);
        write(log_fp, line, strlen(line)-1);
        logged_in = true;

      }

      // Erori de logare
      if (strncmp(holder, "wrong_pin", 9) == 0) {
        printf("IBANK> -3 : Pin  gresit\n");
        write(log_fp, "IBANK> -3 : Pin gresit\n", 24);
      }

      if (strncmp(holder, "wrong_card", 10) == 0) {
        printf("IBANK> -4 : Numar card inexistent\n");
        write(log_fp, "IBANK> -4 : Numar card inexistent\n", 37);
      }

      if (strncmp(holder, "session_active", 14) == 0) {
        printf("IBANK> -2 : Sesiune deja deschisa\n");
        write(log_fp, "IBANK> -2 : Sesiune deja deschisa\n", 35);
      }

      if (strncmp(holder, "locked", 6) == 0) {
        printf("IBANK> -5 : Card blocat\n");
        write(log_fp, "IBANK> -5 : Card blocat\n", 24);
      }

      if (strncmp(holder, "sold", 4) == 0) {
        char amount[20];
        char line[100];
        char *pch;

        pch = strtok(holder, " ");
        pch = strtok (NULL, " ");
        strcpy(amount, pch);
        strcpy(line, "IBANK> ");
        strcat(line, amount);
        strcat(line, "\n");
        printf("%s", line);
        write(log_fp, line, strlen(line));
      }

      if (strncmp(holder, "quit", 4) == 0) {
        close(sock_udp);
        close(sock_tcp);

        printf("Conexiune inchisa de server\n");

        close(log_fp);
        return 0;
      }

      if (strncmp(holder, "logout_accepted", 15) == 0) {
        printf("IBANK> Clientul a fost deconectat\n");
        write(log_fp, "IBANK> Clientul a fost deconectat\n", 35);
        logged_in = false;
      }

      if (strncmp(holder, "logout_failed", 13) == 0) {
        printf("IBANK> -6 : Operatiune esuata\n");
        write(log_fp, "IBANK> -6 : Operatiune esuata\n", 31);
      }

      if (strncmp(holder, "no_founds", 9) == 0) {
        printf("IBANK> -8 : Fonduri insuficiente\n");
        write(log_fp, "IBANK> -8 : Fonduri insuficiente\n", 33);
      }

      // Cream mesajul de confirmare
      if (strncmp(holder, "confirmation", 12) == 0) {
        char line[200];
        char *pch;
        strcpy(line, "Transfer ");
        pch = strtok(holder, " ");
        pch = strtok (NULL, " ");
        strcat(line, pch);

        // daca da numar intreg
        if (strstr(pch, ".") == NULL) {
          strcat(line, ".00");
        }
        strcat(line, " catre ");
        pch = strtok (NULL, " ");
        strcat(line, pch);
        strcat(line, " ");
        pch = strtok (NULL, " ");
        strcat(line, pch);
        strcat(line, "? [y/n]\0");

        printf("%s\n", line);
        write(log_fp, line, strlen(line));
        write(log_fp, "\n", 1);


        transfer_attempt = true;
      }

      if (strncmp(holder, "succes", 6) == 0) {
        printf("IBANK> Transfer realizat cu succes\n");
        write(log_fp, "IBANK> Transfer realizat cu succes\n", 35);
      }

      if (strncmp(holder, "canceled", 8) == 0) {
        printf("IBANK> -9 : Operatie anulata\n");
        write(log_fp, "IBANK> -9 : Operatie anulata\n", 30);
      }
    }
  }


  return 0;
}
