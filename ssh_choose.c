#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ncurses.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_KEYS 50
#define MAX_FILENAME 256
#define MAX_EMAIL 128

typedef struct {
    char filename[MAX_FILENAME];
    char email[MAX_EMAIL];
} SSHKey;

SSHKey keys[MAX_KEYS];
int key_count = 0;
const char *ssh_dir;

/**
 * Checks if a file is a private key based on permissions.
 */
int is_private_key(const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) != 0) return 0;

    // Private SSH keys typically have 600 (rw-------) permissions
    return (st.st_mode & 0777) == 0600;
}

/**
 * Extracts the email address from a public SSH key.
 */
void extract_email_from_pubkey(const char *pubkey_path, char *email) {
    FILE *file = fopen(pubkey_path, "r");
    if (file) {
        char line[512];
        while (fgets(line, sizeof(line), file)) {
            char *email_pos = strrchr(line, ' ');  // Look for last space
            if (email_pos && strchr(email_pos, '@')) {
                strncpy(email, email_pos + 1, MAX_EMAIL);
                email[strcspn(email, "\n")] = 0;  // Remove newline
                fclose(file);
                return;
            }
        }
        fclose(file);
    }
    strcpy(email, "No email found");
}

/**
 * Scans ~/.ssh for private keys.
 */
void get_ssh_keys() {
    DIR *dir;
    struct dirent *entry;
    ssh_dir = getenv("HOME");
    if (!ssh_dir) {
        perror("Could not get HOME environment variable");
        exit(1);
    }
    char ssh_path[MAX_FILENAME];
    snprintf(ssh_path, sizeof(ssh_path), "%s/.ssh", ssh_dir);

    if ((dir = opendir(ssh_path)) == NULL) {
        perror("Could not open ~/.ssh directory");
        exit(1);
    }

    while ((entry = readdir(dir)) != NULL) {
        char key_path[MAX_FILENAME];
        snprintf(key_path, sizeof(key_path), "%s/%s", ssh_path, entry->d_name);

        if (is_private_key(key_path)) {
            if (key_count < MAX_KEYS) {
                strcpy(keys[key_count].filename, entry->d_name);

                // Extract email from corresponding .pub file
                char pub_path[MAX_FILENAME];
                snprintf(pub_path, sizeof(pub_path), "%s/%s.pub", ssh_path, entry->d_name);
                extract_email_from_pubkey(pub_path, keys[key_count].email);

                key_count++;
            }
        }
    }
    closedir(dir);
}

/**
 * Displays the TUI menu.
 */
void display_menu() {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    int highlight = 0;
    int choice = 0;
    int ch;
    int exit_menu = 0;

    while (exit_menu == 0) {
        clear();
        mvprintw(0, 0, "Select an SSH key to add:");
        for (int i = 0; i < key_count; i++) {
            if (i == highlight) attron(A_REVERSE);
            mvprintw(i + 1, 2, "%s (%s)", keys[i].filename, keys[i].email);
            if (i == highlight) attroff(A_REVERSE);
        }
        mvprintw(key_count + 2, 0, "Use arrow keys to select, Enter to confirm, 'q' to quit.");

        ch = getch();
        switch (ch) {
            case KEY_UP:
                highlight = (highlight == 0) ? key_count - 1 : highlight - 1;
                break;
            case KEY_DOWN:
                highlight = (highlight == key_count - 1) ? 0 : highlight + 1;
                break;
            case '\n':  // Enter key
                choice = highlight;
                exit_menu = 1;
                break;
            case 'q':
                endwin();
                exit(0);
        }
    }

    endwin();

    // Run ssh-add on the selected key
    char ssh_key_path[MAX_FILENAME];
    snprintf(ssh_key_path, sizeof(ssh_key_path), "%s/.ssh/%s", ssh_dir, keys[choice].filename);
    printf("Adding SSH key: %s\n", ssh_key_path);
    execlp("ssh-add", "ssh-add", ssh_key_path, (char *)NULL);
}

int main() {
    get_ssh_keys();

    if (key_count == 0) {
        printf("No SSH keys found in ~/.ssh\n");
        return 1;
    }

    display_menu();
    return 0;
}
