#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

// Limitari
#define Max_Clients 30
#define Max_Tries 3
#define Max 1500

// Coduri de eroare
#define Client_Not_Logged -1
#define Session_Active -2
#define Wrong_Pin -3
#define Card_Does_Not_Exist -4
#define Card_Locked -5
#define Failed_Operation -6
#define Failed_Unlocked -7
#define Insufficient_Funds -8
#define Canceled_Operation -9
#define Wrong_Command -10

// Statusul unui card
#define Unlocked true
#define Locked false

// Informatiile unui client
typedef struct {
  char last_name[12];
  char first_name[12];
  int card_number;
  int pin;
  char secret_password[8];
  double amount;
  bool status;
  int tries;
  bool attempt_to_unlock;
}Client;

// Aloca un client
Client *new_client(char *last, char *first, int card, int pin, char *pass,
  double amt) {
  Client *client = (Client *) malloc (sizeof(Client));

  if (!client) {
    return NULL;
  }

  strcpy(client->last_name, last);
  strcpy(client->first_name, first);
  strcpy(client->secret_password, pass);

  client->card_number = card;
  client->pin = pin;
  client->amount = amt;
  client->status = Unlocked;
  client->tries = 0;
  client->attempt_to_unlock = false;

  return client;
}

// Sterge toti clienti pentru eliberarea memoriei
void delete_clients(Client **clients, int number) {
  int counter = 0;
  for (; counter < number; counter += 1) {
    free(clients[counter]);
  }
  free(clients);
}

// Citeste informatiile despre clienti si intoarce un vector
Client **read_from_file(FILE *fp, int number) {
  Client **clients = (Client **) malloc (sizeof(Client*) * number);

  if (!clients) {
    return NULL;
  }

  char first[12], last[12], pass[8];
  int pin, card, counter = 0;
  double amt;

  for (;counter < number; counter += 1) {
    fscanf(fp, "%s %s %u %u %s %lf", last, first, &card, &pin, pass, &amt);
		clients[counter] = new_client(last, first, card, pin, pass, amt);

    // Eliberam memoria alocata in prealabil
    if (clients[counter] == NULL) {
      delete_clients(clients, counter - 1);
      free(clients);
      return NULL;
    }
  }

  fclose(fp);
  return clients;
}

// Intrare in lista de clienti activi
typedef struct Item{
  int card_number;
  int socket_id;
  struct Item *next;
}Item;

// Lista de clienti activi
typedef struct {
  int number;
  struct Item *head;
}List;

// Structura a unui transfer
typedef struct T{
  int socket_id;
  int destination;
  int source;
  double amount;
  struct T *next;
}T;

typedef struct{
  int number;
  struct T *head;
}Transfer_List;


// Aloca un transfer nou
void new_transfer(Transfer_List *list, int socket, int source, int destination,
  double amount){

    T *t = (T*) malloc(sizeof(T));

    if (t == NULL) {
      return;
    }

    t->socket_id = socket;
    t->source = source;
    t->destination = destination;
    t->amount = amount;
    t->next = NULL;

    if (list->number == 0) {
      list->head = t;
    } else {
      T *aux = list->head;
      while (aux->next != NULL) {
        aux = aux->next;
      }
      aux->next = t;
    }

    list->number++;

}

int remove_transfer(Transfer_List *list, int id) {
  T *aux = list->head;
  T *last = NULL;

  while (aux != NULL) {
    if (aux->socket_id == id) {
      T *aux2 = aux;

      if (last != NULL) {
        last->next = aux->next;
      } else {
        list->head = aux->next;
      }

      free(aux2);
      list->number--;
      return 1;
    }
    else {
      last = aux;
      aux = aux->next;
    }
  }
  // A fost parcursa toata lista fara sa se gaseasca clientul activ
  return -1;
}

// Intoarce transfer-ul initiat de pe socket-ul id
T* get_transfer(Transfer_List *list, int id) {
  T *aux = list->head;

  while (aux != NULL) {
    if (aux->socket_id == id) {
      return aux;
    }
    aux = aux->next;
  }

  return false;
}

// Distruge lista de transferuri
void delete_transfer(Transfer_List *list) {
  if (list->head != NULL) {
    T *aux = NULL;
    while (list->head->next = NULL) {
      aux = list->head;
      list->head = list->head->next;
      free(aux);
    }
  }
  free(list);
}

// Aloca un client activ
void new_active_client(List *list, int card, int id) {
  Item *item = (Item *) malloc(sizeof(Item));

  if (item == NULL) {
    return;
  }

  item->card_number = card;
  item->socket_id = id;
  item->next = NULL;

  if (list->number == 0) {
    list->head = item;
  } else {
    Item *aux = list->head;
    while (aux->next != NULL) {
      aux = aux->next;
    }
    aux->next = item;
  }

  list->number++;
}

// Sterge din lista un client activ
int remove_active_client(List *list, int id) {
  Item *aux = list->head;
  Item *last = NULL;

  while (aux != NULL) {
    if (aux->socket_id == id) {
      Item *aux2 = aux;

      if (last != NULL) {
        last->next = aux->next;
      } else {
        list->head = aux->next;
      }

      free(aux2);
      list->number--;
      return 1;
    }
    else {
      last = aux;
      aux = aux->next;
    }
  }
  // A fost parcursa toata lista fara sa se gaseasca clientul activ
  return -1;
}

// Distruge lista de clienti activi
void delete_list(List *list) {
  if (list->head != NULL) {
    Item *aux = NULL;
    while (list->head->next != NULL) {
      aux = list->head;
      list->head = list->head->next;
      free(aux);
    }
  }
  free(list);
}

// Verifica in lista daca userul cu numarul de card este activ
bool checkActive(List *list, int card) {
  Item *aux = list->head;

  while (aux != NULL) {
    if (aux->card_number == card) {
      return true;
    }
    aux = aux->next;
  }

  return false;
}

// Verifica daca exista sesiune activa pe socketul id
bool checkSession(List *list, int id) {
  Item *aux = list->head;

  while (aux != NULL) {
    if (aux->socket_id == id) {
      return true;
    }
    aux = aux->next;
  }

  return false;
}

// Obtine informatiile despre un client cu numarul de card dat
Client *get_information(Client **clients, int card, int number) {
  int counter = 0;

  for (; counter < number; counter += 1) {
    if (clients[counter]->card_number == card) {
      //printf("%d\n",counter);
      return clients[counter];
    }
  }

  return NULL;
}

// Obtine numarul de card dandu-se socketul activ
int get_card(List *list, int id) {
  Item *aux = list->head;

  while (aux != NULL) {
    if (aux->socket_id == id) {
      return aux->card_number;
    }
    aux = aux->next;
  }

  return -1;
}

// Creaza fisierul de log pentru client
void make_log_file(int pid, int *fp) {
  char *filename = (char *) malloc(30);
  char *pid_aux = (char *) malloc(30);

  strcpy(filename, "client-");
  sprintf(pid_aux, "%d", pid);
  strcat(filename, pid_aux);
  strcat(filename, ".log");

  *fp = open(filename, O_WRONLY | O_CREAT, 0644);

  free(pid_aux);
  free(filename);
}
