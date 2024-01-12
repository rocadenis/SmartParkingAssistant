#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include </opt/homebrew/opt/jansson/include/jansson.h>

#define JSON_FILE "data.json"
#define PARK_FILE  "parcare.json"
#define BUFFER_SIZE 20024
#define SERVER_PORT 8080 
#define MAX_CLIENTS 101

//mutex pentru data safety
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct parkingInfo {
    int loc_liber;
    int locuri_libere;
}parkingInfo;
parkingInfo *info = NULL;

typedef struct Client {
    int sd;
    char username[1000];
    char password[1000];
    int isAuthenticated;
    int parkingSpot;
    bool hasSubscription;
    time_t entry_time;
    struct Client *next;
} Client;

Client *head = NULL;

typedef struct ParkingSpot {
    int spotNumber;
    int isOccupied;
    char clientName; // Schimbat de la 'clientname' la 'clientName'
    struct ParkingSpot *next;
} ParkingSpot;

ParkingSpot *spot = NULL;

int readFromJSONFile(char *filename, char *data) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Eroare la deschiderea fișierului JSON");
        return 0;
        
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


//initializare locuri de parcare
void initPark() {
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


//initializare conturi
void deleteAllAccounts() {
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

    // Obține contul "admin" (presupunând că există un cont "admin")
    json_t *adminAccount = json_object_get(root, "admin");
    if (!json_is_object(adminAccount)) {
        fprintf(stderr, "Eroare: Nu s-a găsit contul \"admin\" în fișierul JSON.\n");
        json_decref(root);
        exit(EXIT_FAILURE);
    }

    // Șterge toate conturile, cu excepția contului "admin"
    const char *username;
    json_t *account;
    json_object_foreach(root, username, account) {
        if (strcmp(username, "admin") != 0) {
            json_object_del(root, username);
        }
    }

    // Adaugă înapoi contul "admin"
    json_object_set(root, "admin", adminAccount);

    // Serializează obiectul JSON actualizat și scrie în fișier
    char *serialized = json_dumps(root, JSON_INDENT(4));
    if (!serialized) {
        fprintf(stderr, "Eroare la serializarea obiectului JSON\n");
        free(serialized);
        exit(EXIT_FAILURE);
    }

    writeToJSONFile(JSON_FILE, serialized);

    // Eliberează memoria și eliberează obiectul JSON
    free(serialized);
    json_decref(root);

    printf("Toate conturile au fost șterse din fișierul JSON, cu excepția contului \"admin\".\n");
}


//verificare existenta unui cont
int userExists(const char *username, const char *password) {
    // Încarcă datele din fișierul JSON
    char data[BUFFER_SIZE];
    memset(data,0,BUFFER_SIZE);
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
        return 0;
    }

    // Verifică parola
    const char *jsonPassword = json_string_value(json_object_get(dataStruct, "password"));
    if (strcmp(password, jsonPassword) == 0) {
        json_decref(root);
        return 1; //gasit
    } else {
        // Parola este incorectă
        json_decref(root);
        return 2; 
    }
}


//adaugare cont in file
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
            return "exista";
            continue;
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
    json_object_set_new(newDataObject, "isAuthenticated", json_integer(1));
    json_object_set_new(newDataObject, "parkingSpot", json_integer(0));
    json_object_set_new(newDataObject, "hasSubscription", json_boolean(false));
    json_object_set_new(newDataObject, "entry_time", json_integer(0));

    // Add the new data object to the root object
    json_object_set_new(root, username, newDataObject);

    // Serialize the JSON object and write it to the file
    char *serialized = json_dumps(root, JSON_INDENT(4));
    if (!serialized) {
        fprintf(stderr, "Error serializing JSON object\n");
        
        json_object_clear(newDataObject);
        json_decref(root);

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


//logout account 
void logout(Client *client){
 pthread_mutex_lock(&lock);
  strcpy(client->username, "");
 strcpy(client->password, "");
  client ->isAuthenticated = 0;
  client->parkingSpot = 0;
  client->hasSubscription = false;
  client->entry_time = 0;
pthread_mutex_unlock(&lock);
}


//actulaizare in struct informatiile despre client
struct Client *getUser(Client *client, const char *username,int  client_sock) {
  // Încarcă datele din fișierul JSON
  pthread_mutex_lock(&lock);
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
  //struct Client *client = malloc(sizeof(struct Client));
  //client->sd = json_integer_value(json_object_get(dataStruct, "sd")); // vreau sa pastrez sd ul
  client->sd = client_sock;
  strcpy(client->username, json_string_value(json_object_get(dataStruct, "username")));
  strcpy(client->password, json_string_value(json_object_get(dataStruct, "password")));
  client->isAuthenticated = json_integer_value(json_object_get(dataStruct, "isAuthenticated"));
  client->parkingSpot = json_integer_value(json_object_get(dataStruct, "parkingSpot"));
  client->hasSubscription = json_boolean_value(json_object_get(dataStruct, "hasSubscription"));
  client->entry_time = json_integer_value(json_object_get(dataStruct, "entry_time"));

  // Eliberează memoria
  json_decref(root);
  pthread_mutex_unlock(&lock);      
  return client;
}


//informatii despre locuri de parcare si cel mai apropiar loc liber
void getFreeParkingInfo(parkingInfo *park) {
    // Încarcăm datele din fișierul JSON
    char data[BUFFER_SIZE];
    memset(data,0, BUFFER_SIZE);
    if (!readFromJSONFile(PARK_FILE, data)) {
        perror("Eroare la citirea fișierului JSON");
        exit(EXIT_FAILURE);
    }

    // Parsează datele JSON
    json_error_t error;
    json_t *root = json_loads(data, 0, &error);
    if (!root) {
        fprintf(stderr, "Eroare la parsarea fișierului JSON: %s\n", error.text);
        exit(EXIT_FAILURE);
    }

    // Inițializare la începutul parcajului
    park->loc_liber = 0;
    park->locuri_libere = 0;

    for (int i = 0; i < json_array_size(root); i++) {
        json_t *spotObject = json_array_get(root, i);

        // Verificăm dacă locul este ocupat sau liber
        int isOccupied = json_integer_value(json_object_get(spotObject, "isOccupied"));
        if (isOccupied == 0) {
            // Locul este liber
            park->locuri_libere++;

            // Verificăm dacă acesta este cel mai apropiat loc liber
            int spotNumber = json_integer_value(json_object_get(spotObject, "spotNumber"));
            if (park->loc_liber == 0 || spotNumber < park->loc_liber) {
                park->loc_liber = spotNumber;
            }
        }
    }

    // Eliberăm memoria și eliberăm obiectul JSON
    json_decref(root);
}

//actualizare in file informatiile despre locul de parcare
void updateParkingJSONFile(int spotNumber, char *clientName) {
    // Blochează mutex-ul pentru siguranță la accesul la date
    pthread_mutex_lock(&lock);

    // Încarcă datele din fișierul JSON
    char data[BUFFER_SIZE];
    memset(data, 0, BUFFER_SIZE);
    if (!readFromJSONFile(PARK_FILE, data)) {
        perror("Eroare la citirea fișierului JSON");
        pthread_mutex_unlock(&lock);
        exit(EXIT_FAILURE);
    }

    // Parsează datele JSON
    json_error_t error;
    json_t *root = json_loads(data, 0, &error);
    if (!root) {
        fprintf(stderr, "Eroare la parsarea fișierului JSON in park_file : %s\n", error.text);
        pthread_mutex_unlock(&lock);
        exit(EXIT_FAILURE);
    }

    // Găsește locul de parcare în array-ul JSON și actualizează valorile
    for (int i = 0; i < json_array_size(root); i++) {
        json_t *spotObject = json_array_get(root, i);
        int currentSpotNumber = json_integer_value(json_object_get(spotObject, "spotNumber"));

        if (currentSpotNumber == spotNumber) {
            // Locul de parcare a fost găsit
            json_object_set_new(spotObject, "isOccupied", json_integer(1));
            json_object_set_new(spotObject, "clientName", json_string(clientName));
            break;
        }
    }
        
    // Serializare obiect JSON actualizat
    char *serialized = json_dumps(root, JSON_INDENT(4));
    if (!serialized) {
        fprintf(stderr, "Eroare la serializarea obiectului JSON pentru locurile de parcare\n");
        pthread_mutex_unlock(&lock);
        exit(EXIT_FAILURE);
    }

    // Scrie obiectul JSON actualizat în fișierul cunoscut
    writeToJSONFile(PARK_FILE, serialized);

    // Eliberează memoria și deblochează mutex-ul
    pthread_mutex_unlock(&lock);
    json_decref(root);
    free(serialized);
}


void updateClientJSONFile(Client *client) {
    
    char data[BUFFER_SIZE];
    memset(data,0, BUFFER_SIZE);
    if (!readFromJSONFile(JSON_FILE, data)) {
        perror("Eroare la citirea fișierului JSON");
      
        exit(EXIT_FAILURE);
    }

    json_error_t error;
    json_t *root = json_loads(data, 0, &error);
    if (!root) {
        fprintf(stderr, "Eroare la parsarea fișierului JSON in client_file : %s\n", error.text);
       
        exit(EXIT_FAILURE);
    }

    json_t *userObject = json_object_get(root, client->username);
    if (!json_is_object(userObject)) {
        fprintf(stderr, "Eroare: Utilizatorul nu a fost găsit în fișierul JSON.\n");
        json_decref(root);
       
        exit(EXIT_FAILURE);
    }

    json_object_set_new(userObject, "parkingSpot", json_integer(client->parkingSpot));
    json_object_set_new(userObject, "hasSubscription", json_boolean(client->hasSubscription));
    json_object_set_new(userObject, "entry_time", json_integer(client->entry_time));
    

    char *serialized = json_dumps(root, JSON_INDENT(4));
    if (!serialized) {
        fprintf(stderr, "Eroare la serializarea obiectului JSON\n");
        json_object_clear(userObject);
        json_decref(root);
        
        exit(EXIT_FAILURE);
    }

    if (!writeToJSONFile(JSON_FILE, serialized)) {
        fprintf(stderr, "Eroare la scrierea în fișierul JSON");
        free(serialized);
        json_decref(root);
        
        exit(EXIT_FAILURE);
    }
    free(serialized);
    json_decref(root); 
}

// elibereaza loc de parcare si actualizeaza in client
void freeParkingSpot(int spotNumber, Client *client) {
    // Blochează mutex-ul pentru siguranță la accesul la date
    pthread_mutex_lock(&lock);

    // Încarcă datele din fișierul JSON al locurilor de parcare
    char data[BUFFER_SIZE];
    //memset(data, 0, BUFFER_SIZE);
    if (!readFromJSONFile(PARK_FILE, data)) {
        perror("Eroare la citirea fișierului JSON");
        pthread_mutex_unlock(&lock);
        exit(EXIT_FAILURE);
    }

    // Parsează datele JSON
    json_error_t error;
    json_t *root = json_loads(data, 0, &error);
    if (!root) {
        fprintf(stderr, "Eroare la parsarea fișierului JSON: %s\n", error.text);
        pthread_mutex_unlock(&lock);
        exit(EXIT_FAILURE);
    }

    // Găsește locul de parcare cu numărul dat
    int foundSpotIndex = -1;
    for (int i = 0; i < json_array_size(root); i++) {
        json_t *spotObject = json_array_get(root, i);
        int currentSpotNumber = json_integer_value(json_object_get(spotObject, "spotNumber"));
        int isOccupied = json_integer_value(json_object_get(spotObject, "isOccupied"));

        if (currentSpotNumber == spotNumber && isOccupied) {
            // Locul de parcare ocupat a fost găsit
            foundSpotIndex = i;
            break;
        }
    }

    if (foundSpotIndex != -1) {
        // Actualizează structura locului de parcare pentru a-l elibera
        json_t *foundSpotObject = json_array_get(root, foundSpotIndex);
        json_object_set_new(foundSpotObject, "isOccupied", json_integer(0));
        json_object_set_new(foundSpotObject, "clientName", json_integer(0));

        // Actualizează structura clientului pentru a marca eliberarea locului
        client->parkingSpot = 0;  // sau altă valoare semnificativă pentru loc neocupat
        client->entry_time = 0;
        client->hasSubscription = false;

        // Serializare obiect JSON actualizat
        char *serialized = json_dumps(root, JSON_INDENT(4));
        if (!serialized) {
            fprintf(stderr, "Eroare la serializarea obiectului JSON pentru locurile de parcare\n");
            pthread_mutex_unlock(&lock);
            exit(EXIT_FAILURE);
        }

        // Scrie obiectul JSON actualizat în fișierul cunoscut
        writeToJSONFile(PARK_FILE, serialized);

        // Eliberează memoria și deblochează mutex-ul
        pthread_mutex_unlock(&lock);
        free(serialized);

        updateClientJSONFile(client);

    } else {
        // Tratează cazul în care locul de parcare nu a fost găsit sau este deja liber
        printf("Locul de parcare cu numărul %d nu este ocupat sau nu există.\n", spotNumber);

        // Deblochează mutex-ul
        pthread_mutex_unlock(&lock);
    }

    // Eliberează memoria și eliberează obiectul JSON
    json_decref(root);
}

//calcularea platii pentru parcare
int clientsPayment(Client *client) {
        time_t currentTime;
        time(&currentTime);

        // Calculează diferența de ore și aplică tariful
        int hoursParked = (currentTime - client->entry_time) / 3600; // convertim în ore
        if (hoursParked < 0) {
            // Dacă a intrat după ora curentă, setează la 0
            hoursParked = 1;
        }

        int paymentAmount = hoursParked * 10;

        return paymentAmount;
    }

//procesarea comenzii
void handleCommand(char *command, Client *client, parkingInfo *info, int client_sock) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    char raspuns[BUFFER_SIZE];
    memset(raspuns, 0, sizeof(raspuns));
        
//parasire parcare
    if (strncmp(command, "parasire parcare", 16) == 0) {
            if (client->parkingSpot != 0) {
                int plata;
                if (client->hasSubscription == true){
                    plata = 800;
                }
                else {
                 plata = clientsPayment(client);
                }
            freeParkingSpot(client->parkingSpot, client);

            snprintf(buffer, sizeof(buffer), "Locul a fost eliberat. Aveți de plată %d LEI!", plata);
            send(client_sock, buffer, strlen(buffer), 0);
            memset(buffer, 0 ,BUFFER_SIZE);
        } else {
            
            strncpy(buffer, "Nu ocupati niciun loc. Nu puteti elibera un loc.", sizeof(buffer) - 1);
            send(client_sock, buffer, strlen(buffer), 0);
            memset(buffer, 0 ,BUFFER_SIZE);
        }
//ocupa locul        
    } else if (strncmp(command, "ocupa locul", 11) == 0) {
                if (client->parkingSpot == 0) {

                    snprintf(buffer, sizeof(buffer), "Doriți abonament pe 14 zile sau parcare simplă cu plata de 10 lei/oră? (abonament/plata)");
                    
                    send(client_sock, buffer, strlen(buffer), 0);
                    memset(buffer, 0, BUFFER_SIZE);

                    metodaplata:

                    recv(client_sock, raspuns, sizeof(raspuns), 0);
                    
                    if (strncmp(raspuns, "abonament", 9) == 0) {
                        // Procesează abonamentul pe 14 zile
                        client->entry_time = time(NULL);
                        client->hasSubscription = true;
                        client->parkingSpot = info->loc_liber;
                        updateParkingJSONFile(info->loc_liber, client->username);
                        updateClientJSONFile(client);
                        
                        snprintf(buffer, sizeof(buffer), "Locul %d a fost ocupat. Aveti un abonament pe 14 zile.", info->loc_liber);
                        send(client_sock, buffer, strlen(buffer), 0);
                        memset(buffer, 0, BUFFER_SIZE);

                    } else if (strncmp(raspuns, "plata", 5) == 0) {
                    
                        client->entry_time = time(NULL);
                        client->parkingSpot = info->loc_liber;
                        updateParkingJSONFile(info->loc_liber, client->username);
                        updateClientJSONFile(client);
                        
                        snprintf(buffer, sizeof(buffer), "Locul %d a fost ocupat. Se aplică tariful de 10 lei/oră.", info->loc_liber);
                        send(client_sock, buffer, strlen(buffer), 0);
                        memset(buffer, 0, BUFFER_SIZE);
                            
                    } else {
                        snprintf(buffer, sizeof(buffer), "Răspuns invalid. Vă rugăm să alegeți între abonament și plata pentru parcare.");
                        send(client_sock, buffer, strlen(buffer), 0);
                        memset(buffer, 0, BUFFER_SIZE);
                        goto metodaplata;
                    }
                } else {
                    // Clientul are deja un loc de parcare ocupat
                    
                    snprintf(buffer, sizeof(buffer), "Sunteți deja parcat. Nu puteți ocupa un alt loc.");
                    send(client_sock, buffer, strlen(buffer), 0);
                    memset(buffer, 0, BUFFER_SIZE);
                } 
//quit admin                
    } else if (strncmp(command, "quit server",11) == 0) {
                            // Verifică dacă utilizatorul este admin
                            if (strncmp(client->username, "admin", 5) == 0) {
                                printf("Serverul se închide...\n");
                                logout(client);
                                exit(EXIT_SUCCESS);
                            } else {
                                printf("Doar admin-ul poate închide serverul.\n");
                            }
    } else if (strncmp(command, "quit", 4) == 0){
        printf(" %s a fost deconectat", client->username);
        logout(client);

        snprintf(buffer, sizeof(buffer), "Clientul a fost deconectat");
        send(client_sock, buffer, strlen(buffer), 0);
    } else if (strncmp(command, "locuri de parcare", 17) == 0) {

             getFreeParkingInfo(info);

            snprintf(buffer, sizeof(buffer), "Sunt %d locuri libere. Cel mai apropiat loc liber are numarul %d", info->locuri_libere, info->loc_liber);
            send(client_sock, buffer, strlen(buffer), 0);

//logout
    } else if (strncmp(command, "logout", 6) == 0) {

        logout(client);

        strncpy(buffer, "V-ati deconectat. Se asteapta urmatorul utilizator...", 54);
        send(client_sock, buffer, strlen(buffer), 0);
//init
    } else if (strncmp(command, "init", 4) == 0) {

          char *nume = client->username;

            if (strncmp(nume, "admin", 5) == 0) {
            initPark();
            deleteAllAccounts();

            strncpy(buffer, "Initializare completa", 22);
            send(client_sock, buffer, strlen(buffer), 0);

            } else {
            strncpy(buffer, "ADMIN-ul nu este conectat.", 27);
            send(client_sock, buffer, strlen(buffer), 0);
            }

    } else {

//comanda necunoscuta
        strncpy(buffer, "Comanda necunoscuta!", 21);
        send(client_sock, buffer, strlen(buffer), 0);
    }

    memset(buffer, 0, BUFFER_SIZE);
}

 
 //autentificare si retinere client
void *client_handler(void *arg ) {
    parkingInfo *park = (parkingInfo *)malloc(sizeof(struct parkingInfo));
    Client *client = (Client *)arg;
    int client_sock = client->sd; // Obțineți socket-ul clientului curent
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE] = {0};
    int islogin = 0;
    int isCreateAccount = 0;

        bzero(buffer, BUFFER_SIZE);
        //while problem
restart:
                
    while (client->isAuthenticated == 0) {
        memset(command, 0, BUFFER_SIZE);
        int bytes_received = recv(client_sock, command, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Eroare la primirea comenzii");
            break;
            return NULL;
        }

        bzero(buffer, BUFFER_SIZE);

    if(isCreateAccount == 1){
            char *username = strtok(command, " ");
            char *password = strtok(NULL, "\n");

            if (strlen(password) < 4){
                strncpy(buffer, "Parola este prea scurta! Utilizati minim 4 caractere pentru a creea o parola.\n Introduceți numele și parola pentru contul nou\n ", 130);
                send(client_sock, buffer, strlen(buffer), 0);
                memset(buffer, 0, sizeof(buffer));
                memset(command, 0, sizeof(buffer));
                continue;
            }

            int result = userExists(username, password);
            if ( result == 0 ) {
                addAccountToJson(username,password);

                strncpy(buffer, "Utilizatorul a fost inregistrat!",33);

                getUser(client, username, client_sock);
                islogin = 0;

                send(client_sock, buffer, strlen(buffer), 0);
                memset(buffer, 0, sizeof(buffer));
                memset(command, 0, sizeof(buffer));
                continue;

            } else {
                strncpy(buffer, "Nume de utilizator deja existent! Incercati un alt username.",61);
            }
            send(client_sock, buffer, strlen(buffer), 0);
            memset(command, 0, sizeof(buffer));
            memset(buffer, 0, sizeof(buffer));
            continue;
        }

   if(islogin == 1){
            char *username = strtok(command, " ");
            char *password = strtok(NULL, "\n");
            
            int result = userExists(username, password);

            if ( result == 2 ){
                strncpy(buffer, "Parola incorecta! Introduceți numele și parola pentru cont",61);

            } else if ( result == 1 ) {
                strncpy(buffer, "Logare reușită!",17);

                getUser(client, username, client_sock);
                islogin = 0;

                send(client_sock, buffer, strlen(buffer), 0);
                memset(command, 0, sizeof(command));
                memset(buffer, 0, sizeof(buffer));
                continue;

            } else {
                if (result == 0){
                strncpy(buffer, "Nume de utilizator incorect! Introduceți numele și parola pentru cont",72);
                }
            }
            send(client_sock, buffer, strlen(buffer), 0);
            memset(command, 0, sizeof(buffer));
            memset(buffer, 0, sizeof(buffer));
            continue;
        }

        // Verificăm comanda primită
        if (strncmp(command, "create", 6) == 0) {

            isCreateAccount = 1;
            // Trimitem un mesaj clientului pentru a-l informa că trebuie să introducă numele și parola
            strncpy(buffer, "Introduceți numele și parola pentru contul nou ",50);
            send(client_sock, buffer, strlen(buffer), 0);
            memset(command, 0, sizeof(buffer));
            memset(buffer, 0, BUFFER_SIZE);
            continue;
        
        } else if (strncmp(command, "login", 5) == 0 ) {
            // Resetăm flag-ul de creare a contului
            
            islogin = 1;
            strncpy(buffer, "Introduceți numele și parola pentru cont ",44);
            send(client_sock, buffer, strlen(buffer), 0);
            memset(command, 0, sizeof(command));
            memset(buffer, 0, BUFFER_SIZE);
        
        } else {
            strncpy(buffer, "Comandă necunoscuta!",22);
            send(client_sock, buffer, strlen(buffer), 0);
            memset(command, 0, sizeof(command));
            memset(buffer, 0, BUFFER_SIZE);
            continue;
        }
    } 
        if(client->isAuthenticated == 1){
                 while(client->isAuthenticated == 1){

                     int bytes_received = recv(client_sock, command, BUFFER_SIZE, 0);
                     if (bytes_received <= 0) {
                      perror("Eroare la primirea comenzii");
                        break;
                        return NULL;
                    } else {
                        handleCommand(command, client, park, client_sock);
                       goto restart;
                    }
                }
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
        Client *client = (Client *)malloc(sizeof(struct Client));
        
        if (client == NULL) {
            perror("Eroare la alocarea de memorie pentru Client");
            exit(EXIT_FAILURE);
        }

        client->sd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
        if (client->sd < 0) {
            perror("Eroare la acceptarea conexiunii");
            free(client);
            continue;
        }
            pthread_mutex_lock(&lock);
            client->next = head;
            head = client;
            pthread_mutex_unlock(&lock);

        if (pthread_create(&client_thread, NULL, client_handler, (void *)client) < 0) {
            perror("Eroare la crearea thread-ului");
            free(client);
            return 1;

        }
    }

    return 0;
}
