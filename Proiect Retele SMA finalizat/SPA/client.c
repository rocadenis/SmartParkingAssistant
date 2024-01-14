
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
    {"", ""},
    {"", ""},
    {"Comenzi accesibile dupa autentificare", "comenzi valide"},
    {"", ""},
    {"Parasire parcare", "Leave the parking"},
    {"Parcheaza", "Occupy a parking spot"},
    {"Abonament","Aveti abonament pe 14 zile in valoare de 800 Lei."},
    {"Plata","Sunteti parcat si veti fi taxat cu 10 Lei/ora"},
    {"Cauta un loc de parcare", "Show parking space information"},
    {"Deconectare","Contul utilizatorului a fost deconectat."},
    {"Initializare sistem", "Initialize the parking system"},
    {"", ""},
    {"", ""},
    {"", ""},
    {"", ""},
    {"", ""},
    {"", ""},
    {"", ""},
    {"", ""},
    {"Introduceti comanda aici", ""},
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
    ssize_t received_bytes = recv(data->sockfd, response, BUFFER_SIZE, 0);
    if (received_bytes <= 0) {
        perror("Error receiving response from server");
        close(data->sockfd);
        data->sockfd = -1; // Set the socket descriptor to -1 after closing
        exit(EXIT_FAILURE);
    }

    response[received_bytes] = '\0'; // Null-terminate the received data
    printf("Received %zd bytes: %s\n", received_bytes, response);

    // Display the server response
    append_text(data, g_strconcat(" ", command, NULL));
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
    gtk_window_set_default_size(GTK_WINDOW(data->window), 1000, 600); // Change dimensions as desired

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
    gtk_grid_attach(GTK_GRID(data->grid), submit_button, 1, 2, 1, 1);// 0, 2, 1, 1

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
    const char *test_command = "Introduceti una din comenzile alaturate dupa ce ati\n finalizat autentificarea cu succes.";
    append_text(data, g_strconcat("", test_command, NULL));
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
