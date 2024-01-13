#include <gtk/gtk.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *name_entry;
    GtkWidget *password_entry;
    gchar *result;  // Variable to store the concatenated result
} LoginData;

static void on_submit_clicked(GtkButton *button, LoginData *login_data) {
    const gchar *name = gtk_entry_get_text(GTK_ENTRY(login_data->name_entry));
    const gchar *password = gtk_entry_get_text(GTK_ENTRY(login_data->password_entry));

    // Concatenate name and password
    login_data->result = g_strconcat(name, " ", password, NULL);

    // Print or use the concatenated result as needed
    g_print("Concatenated result: %s\n", login_data->result);

    // Close the window (optional)
    gtk_widget_destroy(login_data->window);
}

static void create_login_window() {
    // Initialize GTK
    gtk_init(NULL, NULL);

    // Create login data structure
    LoginData login_data = {0};

    // Create the main window
    login_data.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(login_data.window), "Login Window");
    gtk_window_set_default_size(GTK_WINDOW(login_data.window), 300, 150);
    g_signal_connect(G_OBJECT(login_data.window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create a grid to organize widgets
    GtkWidget *grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(login_data.window), grid);

    // Create an entry for the name
    login_data.name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(login_data.name_entry), "Enter your name");
    gtk_grid_attach(GTK_GRID(grid), login_data.name_entry, 0, 0, 2, 1);

    // Create an entry for the password
    login_data.password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(login_data.password_entry), "Enter your password");
    gtk_entry_set_visibility(GTK_ENTRY(login_data.password_entry), FALSE); // Hide password
    gtk_grid_attach(GTK_GRID(grid), login_data.password_entry, 0, 1, 2, 1);

    // Create a button for submitting
    GtkWidget *submit_button = gtk_button_new_with_label("Submit");
    g_signal_connect(submit_button, "clicked", G_CALLBACK(on_submit_clicked), &login_data);
    gtk_grid_attach(GTK_GRID(grid), submit_button, 1, 2, 1, 1);

    // Show all widgets
    gtk_widget_show_all(login_data.window);

    // Run the GTK main loop
    gtk_main();
}

int main() {
    create_login_window();
    return 0;
}
