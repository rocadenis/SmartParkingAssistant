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
#define BUFFER_SIZE 2024

typedef struct {
    GtkWidget *window;
    GtkWidget *command_entry;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkWidget *grid;
    int sockfd;
} AppData;

void show_error_dialog(GtkWidget *parent, const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void append_text(GtkTextBuffer *buffer, const gchar *text, GdkRGBA *color) {
    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(buffer, &end_iter);

    if (color) {
        GtkTextTag *tag = gtk_text_buffer_create_tag(buffer, NULL, "foreground-rgba", color, NULL);
        gtk_text_buffer_insert_with_tags(buffer, &end_iter, text, -1, tag, NULL);
    } else {
        gtk_text_buffer_insert(buffer, &end_iter, text, -1);
    }
    gtk_text_buffer_insert(buffer, &end_iter, "\n", -1);
}

void send_command(AppData *data, const gchar *command) {
    char response[BUFFER_SIZE] = {0};

    // Send command to server
    ssize_t sent_bytes = send(data->sockfd, command, strlen(command), 0);
    if (sent_bytes < 0) {
        show_error_dialog(data->window, "Failed to send command to server.");
        return;
    }

    // Receive server response
    ssize_t received_bytes = recv(data->sockfd, response, sizeof(response) - 1, 0);
    if (received_bytes <= 0) {
        show_error_dialog(data->window, "Failed to receive response from server.");
        return;
    }

    response[received_bytes] = '\0'; // Null-terminate response

    // Append sent command and server response to text view
    GdkRGBA command_color = {0.0, 0.5, 0.0, 1.0}; // Green
    GdkRGBA response_color = {1.0, 0.0, 0.0, 1.0};
    append_text(data->buffer, g_strconcat("> ", command, NULL), &command_color);
    append_text(data->buffer, response, &response_color);
}

void on_command_button_clicked(GtkButton *button, AppData *data) {
    const gchar *command = gtk_button_get_label(GTK_WIDGET(button));
    send_command(data, command);
}

void on_submit_clicked(GtkButton *button, AppData *data) {
    const gchar *command = gtk_entry_get_text(GTK_ENTRY(data->command_entry));
    send_command(data, command);
    gtk_entry_set_text(GTK_ENTRY(data->command_entry), ""); // Clear entry
}

void create_gui(AppData *data) {
    GtkWidget *scroll_window, *submit_button, *button_box;

    gtk_init(NULL, NULL);

    // Main window
    data->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(data->window), "Smart Parking Assistant");
    gtk_window_set_default_size(GTK_WINDOW(data->window), 800, 600);
    g_signal_connect(data->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Grid layout
    data->grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(data->window), data->grid);

    // Button box for predefined commands
    button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    const char *commands[] = {"Creare cont", "Autentificare", "Parasire parcare",
                              "Parcheaza", "Abonament", "Plata",
                              "Cauta un loc de parcare", "Deconectare", "Initializare sistem", NULL};

    for (int i = 0; commands[i]; i++) {
        GtkWidget *button = gtk_button_new_with_label(commands[i]);
        g_signal_connect(button, "clicked", G_CALLBACK(on_command_button_clicked), data);
        gtk_box_pack_start(GTK_BOX(button_box), button, TRUE, TRUE, 0);
    }
    gtk_grid_attach(GTK_GRID(data->grid), button_box, 0, 0, 2, 1);

    // Scrollable text view
    scroll_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll_window, TRUE);
    gtk_grid_attach(GTK_GRID(data->grid), scroll_window, 0, 1, 2, 1);

    data->text_view = gtk_text_view_new();
    data->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(data->text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(data->text_view), FALSE);
    gtk_container_add(GTK_CONTAINER(scroll_window), data->text_view);

    // Command entry
    data->command_entry = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(data->grid), data->command_entry, 0, 2, 1, 1);

    // Submit button
    submit_button = gtk_button_new_with_label("Submit");
    g_signal_connect(submit_button, "clicked", G_CALLBACK(on_submit_clicked), data);
    gtk_grid_attach(GTK_GRID(data->grid), submit_button, 1, 2, 1, 1);

    // Connect to server
    data->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (data->sockfd == -1) {
        show_error_dialog(data->window, "Failed to create socket.");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(data->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        show_error_dialog(data->window, "Failed to connect to server.");
        close(data->sockfd);
        exit(EXIT_FAILURE);
    }

    gtk_widget_show_all(data->window);
    gtk_main();
}

int main() {
    AppData data = {0};
    create_gui(&data);
    close(data.sockfd);
    return 0;
}
