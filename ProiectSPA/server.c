#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <jansson.h>

// Definirea unui fișier pentru stocarea datelor în format JSON
#define JSON_FILE "data.json"
#define PARK_FILE  "parcare.json"
#define BUFFER_SIZE 10024
#define SERVER_PORT 8080 // sau orice alt port pe care doriți să-l folosiți
#define MAX_CLIENTS 100

typedef struct Client {
    int sd;
    char username[100];
    char password[100];
    int isAuthenticated;
    int parkingSpot;
    bool hasSubscription;
    time_t entry_time;
} Client;

typedef struct ParkingSpot {
    int spotNumber;
    int isOccupied;
    int clientName; // Schimbat de la 'clientname' la 'clientName'
    struct ParkingSpot *next;
} ParkingSpot;

int online = 0;

// Funcție pentru a serializa structura Client într-un obiect JSON
json_t *serializeClient(const Client *client) {
    json_t *client_object = json_object();

    json_object_set_new(client_object, "username", json_string(client->username));
    json_object_set_new(client_object, "password", json_string(client->password));
    json_object_set_new(client_object, "isAuthenticated", json_integer(client->isAuthenticated));
    json_object_set_new(client_object, "parkingSpot", json_integer(client->parkingSpot));
    json_object_set_new(client_object, "hasSubscription", json_boolean(client->hasSubscription));
    json_object_set_new(client_object, "entry_time", json_integer(client->entry_time));

    return client_object;
}

// Funcție pentru a serializa lista de structuri ParkingSpot într-un obiect JSON
json_t *serializeParkingSpots(ParkingSpot *head) {
    json_t *parking_array = json_array();

    ParkingSpot *current = head;
    while (current != NULL) {
        json_t *spot_object = json_object();
        json_object_set_new(spot_object, "spotNumber", json_integer(current->spotNumber));
        json_object_set_new(spot_object, "isOccupied", json_integer(current->isOccupied));
        json_object_set_new(spot_object, "clientName", json_integer(current->clientName));

        json_array_append_new(parking_array, spot_object);

        current = current->next;
    }

    return parking_array;
}

// Funcție pentru a deserializa obiectul JSON în structura Client
Client deserializeClient(json_t *client_object) {
    Client client;

    strcpy(client.username, json_string_value(json_object_get(client_object, "username")));
    strcpy(client.password, json_string_value(json_object_get(client_object, "password")));
    client.isAuthenticated = json_integer_value(json_object_get(client_object, "isAuthenticated"));
    client.parkingSpot = json_integer_value(json_object_get(client_object, "parkingSpot"));
    client.hasSubscription = json_boolean_value(json_object_get(client_object, "hasSubscription"));
    client.entry_time = json_integer_value(json_object_get(client_object, "entry_time"));

    return client;
}

// Funcție pentru a deserializa obiectul JSON într-o listă de structuri ParkingSpot
ParkingSpot *deserializeParkingSpots(json_t *parking_array) {
    ParkingSpot *head = NULL;
    size_t index;
    json_t *spot_object;

    json_array_foreach(parking_array, index, spot_object) {
        ParkingSpot *spot = (ParkingSpot *)malloc(sizeof(ParkingSpot));
        if (spot == NULL) {
            fprintf(stderr, "Eroare la alocarea de memorie pentru ParkingSpot");
            exit(EXIT_FAILURE);
        }

        spot->spotNumber = json_integer_value(json_object_get(spot_object, "spotNumber"));
        spot->isOccupied = json_integer_value(json_object_get(spot_object, "isOccupied"));
        spot->clientName = json_integer_value(json_object_get(spot_object, "clientName"));
        spot->next = NULL;

        if (head == NULL) {
            head = spot;
        } else {
            ParkingSpot *current = head;
            while (current->next != NULL) {
                current = current->next;
            }
            current->next = spot;
        }
    }

    return head;
}

ParkingSpot *head = NULL;

int readFromJSONFile(char *filename, char *data) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Eroare la deschiderea fișierului JSON");
        return 0;
        exit(EXIT_FAILURE);

    }

    fread(data, sizeof(char), BUFFER_SIZE, file);
    fclose(file);
    return 1;
}
    
int writeToJSONFile(char *filename, char *data) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Eroare la deschiderea fișierului JSON pentru scriere");
        return 0;
    }

    fprintf(file, "%s", data);
    fclose(file);
    return 1;
}

void initializeFile() {
    // Fișierul JSON nu există, creăm unul nou
    char *data = malloc(BUFFER_SIZE * sizeof(char));
    if (data == NULL) {
        perror("Eroare la alocarea memoriei pentru data");
        exit(EXIT_FAILURE);
    }

    if (!readFromJSONFile(PARK_FILE, data)) {
        perror("Eroare la citirea fișierului JSON");
        free(data);
        exit(EXIT_FAILURE);
    }

    // Creare obiect JSON pentru locurile de parcare
    json_t *parking_spots = json_array();

    for (int i = 1; i <= 100; i++) {
        json_t *spot_object = json_object();
        json_object_set_new(spot_object, "spotNumber", json_integer(i));
        json_object_set_new(spot_object, "isOccupied", json_integer(0));
        json_object_set_new(spot_object, "clientName", json_integer(0));

        json_array_append_new(parking_spots, spot_object);
    }

    char *serialized = json_dumps(parking_spots, JSON_INDENT(4));
    if (!serialized) {
        fprintf(stderr, "Eroare la serializarea obiectului JSON pentru locurile de parcare\n");
        free(data);
        exit(EXIT_FAILURE);
    }
    else{
         // Scrierea în fișier
        writeToJSONFile(PARK_FILE, serialized);
    }

    // Eliberarea memoriei
    json_decref(parking_spots);
    free(serialized);
    free(data);
    printf("Am initializat fișierul\n");
}

bool userExists(const char *username, const char *password) {
  // Încarcă datele din fișierul JSON
  char data[BUFFER_SIZE];
  readFromJSONFile(JSON_FILE, data);

  // Parsează datele JSON
  json_error_t error;
  json_t *root = json_loads(data, 0, &error);
  if (!root) {
    fprintf(stderr, "Eroare la parsarea fișierului JSON: %s\n", error.text);
    exit(EXIT_FAILURE);
  }

  // Obține structura de date
  json_t *dataStruct = json_object_get(root, username);
  if (!dataStruct) {
    // Utilizatorul nu există
    json_decref(root);
    return false;
  }

  // Verifică parola
  const char *jsonPassword = json_string_value(json_object_get(dataStruct, "password"));
  if (strcmp(password, jsonPassword) == 0) {
    json_decref(root);
    return true;
  } else {
    // Parola este incorectă
    json_decref(root);
    return false;
  }
}

const char* addAccountToJson(const char *username, const char *password) {
    // Load the data from the JSON file
    char data[BUFFER_SIZE];
    readFromJSONFile(JSON_FILE, data);

    // Parse the JSON data
    json_error_t error;
    json_t *root = json_loads(data, 0, &error);
    if (!root) {
        fprintf(stderr, "Error parsing JSON file: %s\n", error.text);
        exit(EXIT_FAILURE);
    }

    // Check for duplicate usernames in the existing data
    bool foundExistingAccount = false;
    void *iter = json_object_iter(root);
    while(iter) {
        if(strcmp(json_object_iter_key(iter), username) == 0) {
            foundExistingAccount = true;
            break;
        }
        iter = json_object_iter_next(root, iter);
    }

    // If a duplicate username is found, return a message indicating that the username already exists
    if (foundExistingAccount == true) {
        json_decref(root);
        return "exista";
    } else {

    // Create a new data object
    json_t *newDataObject = json_object();
    json_object_set_new(newDataObject, "username", json_string(username));
    json_object_set_new(newDataObject, "password", json_string(password));
    json_object_set_new(newDataObject, "sd", json_integer(0));
    json_object_set_new(newDataObject, "isAuthenticated", json_integer(0));
    json_object_set_new(newDataObject, "parkingSpot", json_integer(0));
    json_object_set_new(newDataObject, "hasSubscription", json_boolean(false));
    json_object_set_new(newDataObject, "entry_time", json_integer(0));

    // Add the new data object to the root object
    json_object_set_new(root, username, newDataObject);

    // Serialize the JSON object and write it to the file
    char *serialized = json_dumps(root, JSON_INDENT(4));
    if (!serialized) {
        fprintf(stderr, "Error serializing JSON object\n");
        exit(EXIT_FAILURE);
    }

    writeToJSONFile(JSON_FILE, serialized);

    // Free the memory
    free(serialized);
    json_decref(root);

    // Return a message indicating that the account was created successfully
    return "creat";
    }
}

struct Client *getUser(const char *username, const char *password) {
  // Încarcă datele din fișierul JSON
  char data[BUFFER_SIZE];
  readFromJSONFile(JSON_FILE, data);

  // Parsează datele JSON
  json_error_t error;
  json_t *root = json_loads(data, 0, &error);
  if (!root) {
    fprintf(stderr, "Eroare la parsarea fișierului JSON: %s\n", error.text);
    exit(EXIT_FAILURE);
  }

  // Obține structura de date
  json_t *dataStruct = json_object_get(root, username);
  if (!dataStruct) {
    // Utilizatorul nu există
    json_decref(root);
    return NULL;
  }

  // Creează structura de date
  struct Client *client = malloc(sizeof(struct Client));
  client->sd = json_integer_value(json_object_get(dataStruct, "sd"));
  strcpy(client->username, json_string_value(json_object_get(dataStruct, "username")));
  strcpy(client->password, json_string_value(json_object_get(dataStruct, "password")));
  client->isAuthenticated = json_integer_value(json_object_get(dataStruct, "isAuthenticated"));
  client->parkingSpot = json_integer_value(json_object_get(dataStruct, "parkingSpot"));
  client->hasSubscription = json_boolean_value(json_object_get(dataStruct, "hasSubscription"));
  client->entry_time = json_integer_value(json_object_get(dataStruct, "entry_time"));

  // Eliberează memoria
  json_decref(root);

  return client;
}

void *client_handler(void *arg) {
    Client *clients = (Client *)arg;
    int client_sock = clients->sd; // Obțineți socket-ul clientului curent
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE] = {0};
    int isCreateAccount = 0; // 1 dacă clientul a introdus comanda "create_account", 0 altfel
    int islogin = 0;
    
    while (online == 0) {

        // Primim comanda de la client
        int bytes_received = recv(client_sock, command, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Eroare la primirea comenzii");
            close(client_sock);
            return NULL;
        }

        // Golim bufferul
        bzero(buffer, BUFFER_SIZE);
        while(isCreateAccount ==1 ){
        
            char *username = strtok(command, " ");
            char *password = strtok(NULL, " ");

            const char *raspuns = addAccountToJson(username, password);

            // Trimitem un mesaj clientului în funcție de rezultat
            if (strcmp(raspuns, "exista")) {

                strcpy(buffer, "Nume de utilizator deja existent! Incercati un alt username.");

            } else{
            
                strcpy(buffer, "Utilizatorul a fost inregistrat!");
                isCreateAccount = 0;
                online = 1;
                struct Client *client = getUser(username, password);
            
            }
            // Trimitem mesajul clientului
            send(client_sock, buffer, strlen(buffer), 0);
            memset(buffer, 0, BUFFER_SIZE);
    break;
    }
    while (islogin == 1){
            // Preluăm numele și parola din comanda curentă
            char *username = strtok(command, " ");
            char *password = strtok(NULL, " ");

            // Verificăm dacă utilizatorul există
            bool userFound = userExists(username, password);
            // Trimitem un mesaj clientului în funcție de rezultat
            if (userFound) {
                strcpy(buffer, "Logare reușită!");
                struct Client *client = getUser(username, password);
                islogin == 0;
                online = 1;
            } else {
                strcpy(buffer, "Nume de utilizator sau parolă incorecte!");
            }
            // Trimitem mesajul clientului
            send(client_sock, buffer, strlen(buffer), 0);
            memset(buffer, 0, sizeof(buffer));
            break;
        }
        if(online == 0){

        // Verificăm comanda primită
        if (strncmp(command, "create", 6) == 0) {
            if (isCreateAccount == 1) {
                strcpy(buffer, "Comandă invalidă! Introduceți o singură comandă.");
                send(client_sock, buffer, strlen(buffer), 0);
                memset(buffer, 0, sizeof(buffer));
                continue;
            }

            isCreateAccount = 1;

            // Trimitem un mesaj clientului pentru a-l informa că trebuie să introducă numele și parola
            strcpy(buffer, "Introduceți numele și parola pentru contul nou: ");
            send(client_sock, buffer, strlen(buffer), 0);
            memset(buffer, 0, BUFFER_SIZE);

        
        } else if (strncmp(command, "login", 5) == 0 ) {
            // Resetăm flag-ul de creare a contului
            
            islogin = 1;
            strcpy(buffer, "Introduceți numele și parola pentru contul: ");
            send(client_sock, buffer, strlen(buffer), 0);
            memset(buffer, 0, BUFFER_SIZE);

        }  else if (strncmp(command, "init", 4) == 0){
            initializeFile();
            strcpy(buffer, "poate a mers");
            send(client_sock, buffer, strlen(buffer), 0);
            memset(buffer, 0, BUFFER_SIZE);
            
        } else {
            strcpy(buffer, "Comandă necunoscuta!");
            send(client_sock, buffer, strlen(buffer), 0);
            continue;
        }
        }

    }
    while(online == 1){
        int bytes_received = recv(client_sock, command, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Eroare la primirea comenzii");
            close(client_sock);
            return NULL;
        }
        strcpy(buffer, "Sunteti online!");
        send(client_sock, buffer, strlen(buffer), 0);
    }

    return NULL;
}



int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    int addrlen = sizeof(server_addr);
    pthread_t client_thread;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la crearea socket-ului");
        exit(EXIT_FAILURE);
    }


    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Eroare la legarea socket la port");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Eroare la ascultarea pentru conexiuni");
        exit(EXIT_FAILURE);
    }

    printf("Serverul este pornit și ascultă pe portul %d\n", SERVER_PORT);
    while (1) {
        Client *clients = (Client *)malloc(sizeof(Client));
        if (clients == NULL) {
            perror("Eroare la alocarea de memorie pentru Client");
            exit(EXIT_FAILURE);
        }

        clients->sd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
        if (clients->sd < 0) {
            perror("Eroare la acceptarea conexiunii");
            free(clients);
            continue;
        }

        if (pthread_create(&client_thread, NULL, client_handler, (void *)clients) < 0) {
            perror("Eroare la crearea thread-ului");
            free(clients);
            return 1;
           
        }
    }

    return 0;
}
