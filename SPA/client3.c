#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 20024

typedef struct {
    GtkWidget *window;
    GtkWidget *command_entry;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkWidget *list_box;
    int sockfd;
    gboolean is_authenticated;
    GtkWidget *grid;
    GtkWidget *entry_username;
    GtkWidget *entry_password;

} AppData;

typedef struct {
    const gchar *name;
    const gchar *description;
    
} CommandInfo;

CommandInfo initial_commands_info[] = {
    {"Creare cont", "Creare cont"},
    {"Autentificare", "Autentificare"},
    {"quit", "quit"},
};

CommandInfo user_commands_info[] = {
    {"Parasire parcare", "Parasire parcare"},
    {"Parcheaza", "Parcheaza"},
    {"quit", "quit"},
    {"Cauta un loc de parcare", "Cauta un loc de parcare"},
    {"Deconectare", "Deconectare."},
};

CommandInfo parking_commands_info[] = {
    {"Plata", "Plata"},
    {"Abonament", "Abonament"},
};

enum UserState {
    STATE_INITIAL,
    STATE_USER_AUTHENTICATED,
    STATE_PARKING,
};

void append_text(AppData *data, const gchar *text);
void send_message(AppData *data, const gchar *message);
void send_welcome_message(AppData *data);
void update_command_description(GtkListBoxRow *row, GtkListBox *list_box, gpointer user_data);
void show_user_commands(AppData *data);
void perform_payment(AppData *data);
void show_subscription_info(AppData *data);
void show_parking_commands(AppData *data);
void show_login_window(AppData *data, gboolean is_registration);
static void create_gui(AppData *data);
static void on_parking_command_clicked(GtkListBoxRow *row, GtkListBox *list_box, AppData *data);
static void on_submit_login_clicked(GtkButton *button, AppData *data);
static void on_initial_button_clicked(GtkListBoxRow *row, GtkListBox *list_box, AppData *data);
static void on_user_command_clicked(GtkListBoxRow *row, GtkListBox *list_box, AppData *data);


void append_text(AppData *data, const gchar *text) {
    GtkTextIter iter;

    // Obține sfârșitul bufferului de text
    gtk_text_buffer_get_end_iter(data->buffer, &iter);

    // Adaugă text la sfârșitul bufferului
    gtk_text_buffer_insert(data->buffer, &iter, text, -1);

    // Rulează bucla principală GTK pentru a actualiza afișajul
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
}

void send_message(AppData *data, const gchar *message) {
    // Verifică dacă socket-ul este deschis
    if (data->sockfd == -1) {
        fprintf(stderr, "Error: Socket is not open.\n");
        return;
    }

    // Trimite mesajul către server
    ssize_t sent_bytes = send(data->sockfd, message, strlen(message), 0);
    if (sent_bytes < 0) {
        perror("Error sending message to server");
        close(data->sockfd);
        data->sockfd = -1;
        exit(EXIT_FAILURE);
    } else {
        printf("Sent %zd bytes: %s\n", sent_bytes, message);
    }

    // Primește și gestionează răspunsul de la server
    char response[BUFFER_SIZE];
    ssize_t received_bytes = recv(data->sockfd, response, BUFFER_SIZE-1, 0);
    if (received_bytes <= 0) {
        perror("Error receiving response from server");
        close(data->sockfd);
        data->sockfd = -1;
        exit(EXIT_FAILURE);
    }

    response[received_bytes] = '\0';
    printf("Received %zd bytes: %s\n", received_bytes, response);

    // Afișează răspunsul în fereastra text
    append_text(data, message); 
    
    append_text(data, response);

}


// Funcția pentru a trimite un mesaj de bun venit și informații despre utilizare
void send_welcome_message(AppData *data) {
    // Mesaj de bun venit personalizat
    const gchar *welcome_message = "Bine ati venit la Smart Parking Assistant!\n"
                                   "Aceasta este o aplicatie pentru asistenta la parcarea inteligenta.\n"
                                   "Pentru a incepe, autentificati-va sau creati un cont.\n";

    // Trimite mesajul de bun venit la server
    send_message(data, welcome_message);
}

void update_command_description(GtkListBoxRow *row, GtkListBox *list_box, gpointer user_data) {
    AppData *data = (AppData *)user_data;

    // Obține indexul rândului selectat
    int index = gtk_list_box_row_get_index(row);

    // Verifică starea curentă a utilizatorului și afișează descrierea corespunzătoare
    if (data->is_authenticated) {
        // Utilizatorul autentificat
        if (index >= 0 && index < G_N_ELEMENTS(user_commands_info)) {
            const gchar *description = user_commands_info[index].description;
            append_text(data, g_strconcat("Command Description: ", description, NULL));
        }
    } else {
        // Starea inițială sau neautentificată
        if (index >= 0 && index < G_N_ELEMENTS(initial_commands_info)) {
            const gchar *description = initial_commands_info[index].description;
            append_text(data, g_strconcat("Command Description: ", description, NULL));
        }
    }
}


// Funcția pentru a gestiona butoanele specifice parcajului
static void on_parking_command_clicked(GtkListBoxRow *row, GtkListBox *list_box, AppData *data) {
    int index = gtk_list_box_row_get_index(row);

    if (index == 0) {
        // Butonul "Plata"
        perform_payment(data);
    } else if (index == 1) {
        // Butonul "Abonament"
        show_subscription_info(data);
    }

    // Actualizează starea utilizatorului
    show_user_commands(data);
}


static void on_submit_clicked(GtkButton *button, AppData *data) {
    const gchar *command = gtk_entry_get_text(GTK_ENTRY(data->command_entry));

    // Send the command to the server and handle the response
    send_message(data, command);

    // Clear the command entry
    gtk_entry_set_text(GTK_ENTRY(data->command_entry), "");
}


// Funcția pentru a afișa fereastra cu autentificare și creare cont
void show_initial_buttons(AppData *data) {
    // Elimină butoanele existente
    gtk_container_foreach(GTK_CONTAINER(data->list_box), (GtkCallback)gtk_widget_destroy, NULL);

    // Adaugă butoanele inițiale
    for (int i = 0; i < G_N_ELEMENTS(initial_commands_info); i++) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(initial_commands_info[i].name);
        gtk_container_add(GTK_CONTAINER(row), label);
        gtk_list_box_insert(data->list_box, row, -1);
        g_signal_connect(row, "activate", G_CALLBACK(on_initial_button_clicked), data->list_box);
    }

    // ...

    // Afișează toate widgeturile
    gtk_widget_show_all(data->window);
}


void show_user_commands(AppData *data) {
    // Elimină butoanele existente
    gtk_container_foreach(GTK_CONTAINER(data->list_box), (GtkCallback)gtk_widget_destroy, NULL);

    // Adaugă butoanele specifice utilizatorului
    for (int i = 0; i < G_N_ELEMENTS(user_commands_info); i++) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(user_commands_info[i].name);
        gtk_container_add(GTK_CONTAINER(row), label);
        gtk_list_box_insert(data->list_box, row, -1);
        g_signal_connect(row, "activate", G_CALLBACK(on_user_command_clicked), data->list_box);
    }

    // Afișează toate widgeturile
    gtk_widget_show_all(data->window);
}

// Funcția pentru a gestiona comanda "Plata"
void perform_payment(AppData *data) {
    // Poți implementa aici logica specifică pentru plata
    const char *payment_command = "Plata";
    send_message(data, payment_command);  // Trimite comanda către server

}

// Funcția pentru a gestiona comanda "Abonament"
void show_subscription_info(AppData *data) {
    // Poți implementa aici logica specifică pentru afișarea informațiilor despre abonament
    const char *subscription_command = "Abonament";
    send_message(data, subscription_command);  // Trimite comanda către server
}


void show_parking_commands(AppData *data) {
    // Elimină butoanele existente
    send_message(data, "Cauta un loc de parcare");
    gtk_container_foreach(GTK_CONTAINER(data->list_box), (GtkCallback)gtk_widget_destroy, NULL);

    // Adaugă butoanele specifice parcajului
    for (int i = 0; i < G_N_ELEMENTS(parking_commands_info); i++) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(parking_commands_info[i].name);
        gtk_container_add(GTK_CONTAINER(row), label);
        gtk_list_box_insert(data->list_box, row, -1);
        g_signal_connect(row, "activate", G_CALLBACK(on_parking_command_clicked), data->list_box);
    }

    // Afișează toate widgeturile
    gtk_widget_show_all(data->window);
}

// Funcția pentru a afișa fereastra pentru introducerea numelui de utilizator și parolei
void show_login_window(AppData *data, gboolean is_registration) {
    GtkWidget *login_window, *grid, *label_username, *label_password, *submit_button;

   const gchar *username = gtk_entry_get_text(GTK_ENTRY(data->entry_username));
const gchar *password = gtk_entry_get_text(GTK_ENTRY(data->entry_password));

        // Afișează un avertisment în interfața utilizatorului
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(data->window),
                                                GTK_DIALOG_MODAL,
                                                GTK_MESSAGE_WARNING,
                                                GTK_BUTTONS_OK,
                                                "Numele de utilizator și parola sunt obligatorii! Reintroduceți datele.");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;


    // Creează fereastra de login
    login_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(login_window), is_registration ? "Creare cont" : "Autentificare");
    gtk_window_set_default_size(GTK_WINDOW(login_window), 300, 200);
    g_signal_connect(G_OBJECT(login_window), "destroy", G_CALLBACK(gtk_widget_destroy), login_window);

    // Creează grid pentru organizarea widget-urilor
    grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(login_window), grid);

label_username = gtk_label_new("Nume de utilizator:");
data->entry_username = gtk_entry_new();
label_password = gtk_label_new("Parolă:");
data->entry_password = gtk_entry_new();
gtk_entry_set_visibility(GTK_ENTRY(data->entry_password), FALSE);
gtk_entry_set_invisible_char(GTK_ENTRY(data->entry_password), '*');

gtk_grid_attach(GTK_GRID(grid), label_username, 0, 0, 1, 1);
gtk_grid_attach(GTK_GRID(grid), data->entry_username, 1, 0, 1, 1);
gtk_grid_attach(GTK_GRID(grid), label_password, 0, 1, 1, 1);
gtk_grid_attach(GTK_GRID(grid), data->entry_password, 1, 1, 1, 1);

    // Adaugă butonul de submit
    submit_button = gtk_button_new_with_label(is_registration ? "Creare cont" : "Autentificare");
    g_signal_connect(submit_button, "clicked", G_CALLBACK(on_submit_login_clicked), data);
    gtk_grid_attach(GTK_GRID(grid), submit_button, 0, 2, 2, 1);

    // Arată toate widget-urile
    gtk_widget_show_all(login_window);
}

// Funcția pentru a gestiona evenimentul de submit din fereastra de login
static void on_submit_login_clicked(GtkButton *button, AppData *data) {
    // Obține numele de utilizator și parola din câmpurile de introducere
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(data->entry_username));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(data->entry_password));

    // Verifică dacă numele de utilizator și parola sunt goale
    if (g_strcmp0(username, "") == 0 || g_strcmp0(password, "") == 0) {
        send_message(data, "Numele de utilizator și parola sunt obligatorii!");
        return;
    }
    gtk_widget_destroy(GTK_WIDGET(button));
}

// Funcția pentru a gestiona butoanfele inițiale
static void on_initial_button_clicked(GtkListBoxRow *row, GtkListBox *list_box, AppData *data) {
    GtkWidget *label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(row)));
    const gchar *button_name = gtk_label_get_text(GTK_LABEL(label));

    // Trimite comanda specifică butonului la server
    send_message(data, button_name);

    // Dacă este apăsat butonul "Creare cont"
    if (g_strcmp0(button_name, "Creare cont") == 0) {
        // Deschide fereastra pentru introducerea numelui de utilizator și parolei
        show_login_window(data, TRUE); // TRUE indică că se creează un cont
    }

    // Dacă este apăsat butonul "Autentificare"
    else if (g_strcmp0(button_name, "Autentificare") == 0) {
        // Deschide fereastra pentru introducerea numelui de utilizator și parolei
        show_login_window(data, FALSE); // FALSE indică că se autentifică
    } else if (g_strcmp0(button_name, "quit") == 0) {
        // Implement logic for "quit"
        data->is_authenticated = FALSE;
        gtk_widget_destroy(data->window);
    }

    update_command_description(row, list_box, data);
}

static void on_user_command_clicked(GtkListBoxRow *row, GtkListBox *list_box, AppData *data) {
    GtkWidget *label = GTK_LABEL(gtk_bin_get_child(GTK_BIN(row)));
    const gchar *button_name = gtk_label_get_text(label);

    if (g_strcmp0(button_name, "locuri de parcare") == 0) {
        send_message(data, button_name);
    } else if (g_strcmp0(button_name, "Parasire parcare") == 0) {
        send_message(data, button_name);
    } else if (g_strcmp0(button_name, "Parcheaza") == 0) {
        send_message(data, button_name);
        show_parking_commands(data);
    } else if (g_strcmp0(button_name, "Cauta un loc de parcare") == 0) {
        send_message(data, button_name);
    } else if (g_strcmp0(button_name, "Deconectare") == 0) {
        // Implement logic for "Deconectare"
        data->is_authenticated = FALSE;
        show_initial_buttons(data);
    }

    update_command_description(row, list_box, data);
}

// Funcția de creare a interfeței grafice
static void create_gui(AppData *data) {
    GtkWidget *scroll_window;

    // Initializează GTK
    gtk_init(NULL, NULL);

    // Creează fereastra principală
    data->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(data->window), "Smart Parking Assistant");

    // Setează dimensiunea implicită a ferestrei
    gtk_window_set_default_size(GTK_WINDOW(data->window), 800, 600);

    // Conectează semnalul de distrugere
    g_signal_connect(G_OBJECT(data->window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Creează o grilă pentru organizarea widgeturilor
    data->grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(data->window), data->grid);

    // Creează o listă de comenzi
    data->list_box = gtk_list_box_new();
    gtk_grid_attach(GTK_GRID(data->grid), data->list_box, 0, 0, 1, 2);

    // Adaugă rânduri în listă pentru butoanele inițiale
    for (int i = 0; i < G_N_ELEMENTS(initial_commands_info); i++) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(initial_commands_info[i].name);
        gtk_container_add(GTK_CONTAINER(row), label);
        gtk_list_box_insert(data->list_box, row, -1);
        g_signal_connect(row, "activate", G_CALLBACK(on_initial_button_clicked), data->list_box);
    }

    // Creează un panou de derulare pentru vizualizarea textului
    scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    // Setează proprietățile panoului de derulare
    gtk_widget_set_hexpand(scroll_window, TRUE);
    gtk_widget_set_vexpand(scroll_window, TRUE);

    gtk_grid_attach(GTK_GRID(data->grid), scroll_window, 1, 0, 1, 2);

    // Creează un text view
    data->text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(data->text_view), FALSE);

    // Setează proprietățile text view
    gtk_widget_set_hexpand(data->text_view, TRUE);
    gtk_widget_set_vexpand(data->text_view, TRUE);

    

    gtk_container_add(GTK_CONTAINER(scroll_window), data->text_view);

    #include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 20024

typedef struct {
    GtkWidget *window;
    GtkWidget *command_entry;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkWidget *list_box;
    int sockfd; // Add the socket descriptor
    gboolean is_authenticated; // Flag to check if the client is authenticated
    GtkWidget *grid;
} AppData;

typedef struct {
    const gchar *name;
    const gchar *description;
} CommandInfo;

CommandInfo commands_info[] = {
    {"Creare cont", "Create a new account"},
    {"Autentificare", "Log in to an existing account"},
    {"locuri de parcare", "Check the current parking status"},
    {"Parasire parcare", "Leave the parking"},
    {"Parcheaza", "Occupy a parking spot"},
    {"Abonament","Aveti abonament pe 14 zile in valoare de 800 Lei."},
    {"Plata","Sunteti parcat si veti fi taxat cu 10 Lei/ora"},
    {"quit", "Disconnect from the server"},
    {"Cauta un loc de parcare", "Show parking space information"},
    {"Deconectare","Contul utilizatorului a fost deconectat."},
    {"Initializare sistem", "Initialize the parking system"},
};


void append_text(AppData *data, const gchar *text) {
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(data->buffer, &iter);
    gtk_text_buffer_insert(data->buffer, &iter, text, -1);
    gtk_text_buffer_insert(data->buffer, &iter, "\n", -1);
}

void update_command_description(GtkListBoxRow *row, GtkListBox *list_box) {
    int index = gtk_list_box_row_get_index(row);
    AppData *data = (AppData *)g_object_get_data(G_OBJECT(list_box), "app_data");

    // Clear the current text in the text view
    gtk_text_buffer_set_text(data->buffer, "", -1);

    // Display the description of the selected command
    append_text(data, g_strconcat("Command: ", commands_info[index].name, NULL));
    append_text(data, g_strconcat("Description: ", commands_info[index].description, NULL));
}

void send_command(AppData *data, const gchar *command) {
    // Check if the socket is still open
    if (data->sockfd == -1) {
        fprintf(stderr, "Error: Socket is not open.\n");
        return;
    }

    // Send the command to the server (data->sockfd)
    ssize_t sent_bytes = send(data->sockfd, command, strlen(command), 0);
    if (sent_bytes < 0) {
        perror("Error sending command to server");
        close(data->sockfd);
        data->sockfd = -1; // Set the socket descriptor to -1 after closing
        exit(EXIT_FAILURE);
    } else {
        printf("Sent %zd bytes: %s\n", sent_bytes, command);
    }

    // Receive and handle the server response
    char response[BUFFER_SIZE];
    ssize_t received_bytes = recv(data->sockfd, response, BUFFER_SIZE - 1, 0);
    if (received_bytes <= 0) {
        perror("Error receiving response from server");
        close(data->sockfd);
        data->sockfd = -1; // Set the socket descriptor to -1 after closing
        exit(EXIT_FAILURE);
    }

    response[received_bytes] = '\0'; // Null-terminate the received data
    printf("Received %zd bytes: %s\n", received_bytes, response);

    // Display the server response
    append_text(data, g_strconcat("Command: ", command, NULL));
    append_text(data, g_strconcat(" ", response, NULL));

    // Handle specific responses
    // (You need to implement the logic based on your server responses)
}

static void on_submit_clicked(GtkButton *button, AppData *data) {
    const gchar *command = gtk_entry_get_text(GTK_ENTRY(data->command_entry));

    // Send the command to the server and handle the response
    send_command(data, command);

    // Clear the command entry
    gtk_entry_set_text(GTK_ENTRY(data->command_entry), "");
}

static void create_gui(AppData *data) {
    GtkWidget *submit_button, *scroll_window;

    // Initialize GTK
    gtk_init(NULL, NULL);

    // Create the main window
    data->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(data->window), "Smart Parking Assistant");

    // Set the default size of the window
    gtk_window_set_default_size(GTK_WINDOW(data->window), 800, 600); // Change dimensions as desired

    // Connect the destroy signal
    g_signal_connect(G_OBJECT(data->window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create a grid to organize widgets
    data->grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(data->window), data->grid);

    // Create a list box for commands
    data->list_box = gtk_list_box_new();
    gtk_grid_attach(GTK_GRID(data->grid), data->list_box, 0, 0, 1, 2);

    // Add rows to the list box
    for (int i = 0; i < G_N_ELEMENTS(commands_info); i++) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(commands_info[i].name);
        gtk_container_add(GTK_CONTAINER(row), label);
        gtk_list_box_insert(data->list_box, row, -1);
        g_signal_connect(row, "activate", G_CALLBACK(update_command_description), data->list_box);
    }

    // Create a scrolled window for the text view
    scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // Set the scrolled window properties
    gtk_widget_set_hexpand(scroll_window, TRUE);
    gtk_widget_set_vexpand(scroll_window, TRUE);
    
    gtk_grid_attach(GTK_GRID(data->grid), scroll_window, 1, 0, 1, 2);

    // Create a text view
    data->text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(data->text_view), FALSE);
    
    // Set the text view properties
    gtk_widget_set_hexpand(data->text_view, TRUE);
    gtk_widget_set_vexpand(data->text_view, TRUE);
    
    gtk_container_add(GTK_CONTAINER(scroll_window), data->text_view);

    // Create an entry for commands
    data->command_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(data->grid), data->command_entry, 0, 2, 1, 1);

    // Create a button for submitting commands
    submit_button = gtk_button_new_with_label("Submit");
    g_signal_connect(submit_button, "clicked", G_CALLBACK(on_submit_clicked), data);
    gtk_grid_attach(GTK_GRID(data->grid), submit_button, 1, 2, 1, 1);

    // Set up the text buffer for the text view
    data->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->text_view));

    // Connect to the server
    data->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (data->sockfd == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    if (connect(data->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to server");
        close(data->sockfd);
        exit(EXIT_FAILURE);
    }

    // Set the application data for the list box
    g_object_set_data(G_OBJECT(data->list_box), "app_data", data);

    // Test command to verify the connection
    const char *test_command = "test_command";
    send_command(data, test_command);

    // Show all widgets
    gtk_widget_show_all(data->window);

    // Run the GTK main loop
    gtk_main();
}



int main() {
    AppData data = {0};
    create_gui(&data);
    close(data.sockfd); // Close the socket when the application exits
    return 0;
}


    // Conectează funcția on_user_command_clicked la activarea câmpului de comandă
    g_signal_connect(data->command_entry, "activate", G_CALLBACK(on_initial_button_clicked), data->list_box);

    // Setează bufferul de text pentru text view
    data->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->text_view));

   // Conectează-te la server
data->sockfd = socket(AF_INET, SOCK_STREAM, 0);
if (data->sockfd == -1) {
    perror("Eroare la crearea socketului");
    exit(EXIT_FAILURE);
}

struct sockaddr_in server_addr;
server_addr.sin_family = AF_INET;
server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
server_addr.sin_port = htons(SERVER_PORT);

if (connect(data->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("Eroare la conectarea la server");
    close(data->sockfd);
    exit(EXIT_FAILURE);
}

// Trimite mesajul de bun venit și informații despre utilizare
send_welcome_message(data);


    // Afișează toate widgeturile
    gtk_widget_show_all(data->window);

    // Rulează bucla principală GTK
    gtk_main();
}

int main() {
    AppData data = {0};
    create_gui(&data);

    // Check if the socket is valid before attempting to read data
    if (data.sockfd != -1) {
        while (TRUE) {
            // Read data from the socket
            char buffer[BUFFER_SIZE];
            ssize_t bytes_read = recv(data.sockfd, buffer, sizeof(buffer), 0);

            if (bytes_read == 0) {
                // Connection closed by server
                break;
            } else if (bytes_read < 0) {
                // Error reading data
                perror("Error reading from socket");
                close(data.sockfd);
                break;
            }

            // Process the received data
            append_text(&data, buffer);
        }
    } else {
        // Socket is not valid
        fprintf(stderr, "Error: Socket is not valid\n");
    }

    close(data.sockfd); // Close the socket when the application exits
    return 0;
}
