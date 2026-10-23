/*
 * Author: Harsh (Refactored to v2.0)
 * Language: C (C99 / C11 standard, zero external dependencies)
 *
 * Features Included:
 *   1. Add book information (supports multi-word titles/authors & duplicate ID checks)
 *   2. Display all books in a formatted terminal table
 *   3. Search books by author (case-insensitive substring match)
 *   4. Search a specific book by title or accession number & view full details
 *   5. Issue or return a book (toggles availability status)
 *   6. Delete a book record
 *   7. Sort books by Accession Number, Title, or Price (overflow-safe qsort)
 *   8. Library Analytics: Total vs. In-Library vs. Issued counts, Total Valuation,
 *      and Automated Joining Fee recommendation
 *   9. Automatic Binary File Persistence (library.dat)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DB_FILENAME   "library.dat"
#define MAX_TITLE     80
#define MAX_AUTHOR    60
#define INITIAL_CAP   16

#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_CYAN    "\033[36m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_RED     "\033[31m"

typedef struct {
    int accession_num;
    char title[MAX_TITLE];
    char author[MAX_AUTHOR];
    double price;
    int is_issued; // 0 = In Library; 1 = Issued 
} Book;

typedef struct {
    Book *records;
    size_t count;
    size_t capacity;
} Library;

void read_string(const char *prompt, char *buffer, size_t size) {
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(buffer, (int)size, stdin) == NULL) {
            clearerr(stdin);
            printf("\n");
            continue;
        }
        if (strchr(buffer, '\n') == NULL) {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
        } 
        else 
            buffer[strcspn(buffer, "\r\n")] = '\0';
        char *start = buffer;
        while (*start && isspace((unsigned char)*start)) 
            start++;
        if (start != buffer) 
            memmove(buffer, start, strlen(start) + 1);
        if (strlen(buffer) > 0) {
            return;
        }
        printf(CLR_RED "  [!] Input cannot be empty. Please try again.\n" CLR_RESET);
    }
}

int read_int(const char *prompt, int min_val, int max_val) {
    char buf[64];
    while (1) {
        read_string(prompt, buf, sizeof(buf));
        char *endptr;
        long val = strtol(buf, &endptr, 10);
        while (*endptr && isspace((unsigned char)*endptr)) 
            endptr++;
        if (*endptr == '\0' && val >= min_val && val <= max_val) 
            return (int)val;
        printf(CLR_RED "  [!] Please enter a valid integer between %d and %d.\n" CLR_RESET, min_val, max_val);
    }
}

double read_double(const char *prompt, double min_val) {
    char buf[64];
    while (1) {
        read_string(prompt, buf, sizeof(buf));
        char *endptr;
        double val = strtod(buf, &endptr);
        while (*endptr && isspace((unsigned char)*endptr)) 
            endptr++;
        if (*endptr == '\0' && val >= min_val) 
            return val;
        printf(CLR_RED "  [!] Please enter a valid number (>= %.2f).\n" CLR_RESET, min_val);
    }
}

int contains_case_insensitive(const char *haystack, const char *needle) {
    if (!*needle) return 1;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++;
            n++;
        }
        if (!*n) return 1;
    }
    return 0;
}


void lib_init(Library *lib) {
    lib->count = 0;
    lib->capacity = INITIAL_CAP;
    lib->records = malloc(lib->capacity * sizeof(Book));
    if (!lib->records) {
        fprintf(stderr, "Fatal: Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
}

void lib_ensure_capacity(Library *lib) {
    if (lib->count >= lib->capacity) {
        size_t new_cap = lib->capacity * 2;
        Book *temp = realloc(lib->records, new_cap * sizeof(Book));
        if (!temp) {
            fprintf(stderr, "Fatal: Memory reallocation failed.\n");
            free(lib->records);
            exit(EXIT_FAILURE);
        }
        lib->records = temp;
        lib->capacity = new_cap;
    }
}

void lib_free(Library *lib) {
    free(lib->records);
    lib->records = NULL;
    lib->count = 0;
    lib->capacity = 0;
}

int lib_save(const Library *lib, const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("  [!] Failed to open database file for saving");
        return 0;
    }
    if (fwrite(&lib->count, sizeof(size_t), 1, fp) != 1) {
        fclose(fp);
        return 0;
    }
    if (lib->count > 0) {
        if (fwrite(lib->records, sizeof(Book), lib->count, fp) != lib->count) {
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return 1;
}

void lib_load(Library *lib, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) 
        return; // Database not found
    size_t saved_count = 0;
    if (fread(&saved_count, sizeof(size_t), 1, fp) != 1) {
        fclose(fp);
        return;
    }
    while (lib->capacity < saved_count) {
        lib_ensure_capacity(lib);
        if (lib->capacity < saved_count) {
            lib->capacity *= 2;
            lib->records = realloc(lib->records, lib->capacity * sizeof(Book));
        }
    }
    size_t read_items = fread(lib->records, sizeof(Book), saved_count, fp);
    lib->count = read_items;
    fclose(fp);
}


int cmp_accession(const void *a, const void *b) {
    const Book *ba = (const Book *)a;
    const Book *bb = (const Book *)b;
    return (ba->accession_num > bb->accession_num) - (ba->accession_num < bb->accession_num);
}

int cmp_title(const void *a, const void *b) {
    const Book *ba = (const Book *)a;
    const Book *bb = (const Book *)b;
    return strcmp(ba->title, bb->title);
}

int cmp_price(const void *a, const void *b) {
    const Book *ba = (const Book *)a;
    const Book *bb = (const Book *)b;
    return (ba->price > bb->price) - (ba->price < bb->price);
}

void print_table_header(void) {
    printf(CLR_BOLD "+----------+----------------------------------+--------------------------+------------+-------------+\n");
    printf("| %-8s | %-32s | %-24s | %-10s | %-11s |\nAcc. No", "Title", "Author", "Price", "Status");
    printf("+----------+----------------------------------+--------------------------+------------+-------------+" CLR_RESET "\n");
}

void print_book_row(const Book *b) {
    const char *status_str = b->is_issued ? CLR_YELLOW "ISSUED     " CLR_RESET : CLR_GREEN  "IN LIBRARY " CLR_RESET;
    printf("| %-8d | %-32.32s | %-24.24s | %10.2f | %s |\n", b->accession_num, b->title, b->author, b->price, status_str);
}

void print_table_footer(void) {
    printf(CLR_BOLD "+----------+----------------------------------+--------------------------+------------+-------------+" CLR_RESET "\n");
}

void print_book_card(const Book *b) {
    printf("\n" CLR_CYAN "  +--------------------------------------------------------+\n" CLR_RESET);
    printf("  | " CLR_BOLD "Accession Number :" CLR_RESET " %-35d |\n", b->accession_num);
    printf("  | " CLR_BOLD "Title            :" CLR_RESET " %-35.35s |\n", b->title);
    printf("  | " CLR_BOLD "Author           :" CLR_RESET " %-35.35s |\n", b->author);
    printf("  | " CLR_BOLD "Price            :" CLR_RESET " %-35.2f |\n", b->price);
    printf("  | " CLR_BOLD "Current Status   :" CLR_RESET " %-35s |\n", b->is_issued ? "Issued to Member" : "Available in Library");
    printf(CLR_CYAN "  +--------------------------------------------------------+" CLR_RESET "\n");
}

int find_index_by_accession(const Library *lib, int acc_num) {
    for (size_t i = 0; i < lib->count; i++)
        if (lib->records[i].accession_num == acc_num) 
            return (int)i;
    return -1;
}

void add_book(Library *lib) {
    lib_ensure_capacity(lib);
    Book b;
    printf(CLR_CYAN "\n--- Add New Book ---\n" CLR_RESET);
    while (1) {
        b.accession_num = read_int("  Accession Number (1 - 999999): ", 1, 999999);
        if (find_index_by_accession(lib, b.accession_num) != -1) 
            printf(CLR_RED "  [!] A book with Accession Number %d already exists.\n" CLR_RESET, b.accession_num);
        else 
            break;
    }
    read_string("  Book Title  : ", b.title, sizeof(b.title));
    read_string("  Author Name : ", b.author, sizeof(b.author));
    b.price = read_double("  Book Price  : ", 0.0);
    b.is_issued = read_int("  Status (0 = In Library, 1 = Issued): ", 0, 1);
    lib->records[lib->count++] = b;
    lib_save(lib, DB_FILENAME);
    printf(CLR_GREEN "  [+] Book \"%s\" added and saved to %s.\n" CLR_RESET, b.title, DB_FILENAME);
}

void display_all_books(const Library *lib) {
    if (lib->count == 0) {
        printf(CLR_YELLOW "\n  [i] The library currently has no book records.\n" CLR_RESET);
        return;
    }
    printf("\n");
    print_table_header();
    for (size_t i = 0; i < lib->count; i++) 
        print_book_row(&lib->records[i]);
    print_table_footer();
}

void search_by_author(const Library *lib) {
    if (lib->count == 0) {
        printf(CLR_YELLOW "\n  [i] Library is empty.\n" CLR_RESET);
        return;
    }
    char query[MAX_AUTHOR];
    read_string("\n  Enter author name (or partial name): ", query, sizeof(query));
    size_t matches = 0;
    for (size_t i = 0; i < lib->count; i++) {
        if (contains_case_insensitive(lib->records[i].author, query)) {
            if (matches == 0) {
                printf("\n");
                print_table_header();
            }
            print_book_row(&lib->records[i]);
            matches++;
        }
    }
    if (matches > 0) {
        print_table_footer();
        printf(CLR_GREEN "  [i] Found %zu book(s) matching author \"%s\".\n" CLR_RESET, matches, query);
    } 
    else 
        printf(CLR_RED "  [!] No records found for author matching \"%s\".\n" CLR_RESET, query);
}

void search_specific_book(const Library *lib) {
    if (lib->count == 0) {
        printf(CLR_YELLOW "\n  [i] Library is empty.\n" CLR_RESET);
        return;
    }
    printf(CLR_CYAN "\n--- Search Specific Book ---\n" CLR_RESET);
    printf("  1. Search by Accession Number (Exact)\n");
    printf("  2. Search by Title (Partial / Case-Insensitive)\n");
    int mode = read_int("  Select search mode (1-2): ", 1, 2);
    if (mode == 1) {
        int acc = read_int("  Enter Accession Number: ", 1, 999999);
        int idx = find_index_by_accession(lib, acc);
        if (idx != -1) 
            print_book_card(&lib->records[idx]);
        else 
            printf(CLR_RED "  [!] No book found with Accession Number %d.\n" CLR_RESET, acc);
    } 
    else {
        char query[MAX_TITLE];
        read_string("  Enter Book Title keyword: ", query, sizeof(query));
        size_t matches = 0;
        for (size_t i = 0; i < lib->count; i++) {
            if (contains_case_insensitive(lib->records[i].title, query)) {
                print_book_card(&lib->records[i]);
                matches++;
            }
        }
        if (matches == 0) 
            printf(CLR_RED "  [!] No book found matching title \"%s\".\n" CLR_RESET, query);
    }
}

void toggle_issue_status(Library *lib) {
    if (lib->count == 0) {
        printf(CLR_YELLOW "\n  [i] Library is empty.\n" CLR_RESET);
        return;
    }
    int acc = read_int("\n  Enter Accession Number to Issue/Return: ", 1, 999999);
    int idx = find_index_by_accession(lib, acc);
    if (idx == -1) {
        printf(CLR_RED "  [!] Book with Accession Number %d not found.\n" CLR_RESET, acc);
        return;
    }
    Book *b = &lib->records[idx];
    b->is_issued = !b->is_issued;
    lib_save(lib, DB_FILENAME);
    printf(CLR_GREEN "  [+] \"%s\" is now marked as: %s\n" CLR_RESET, b->title, b->is_issued ? "ISSUED" : "IN LIBRARY (Available)");
}

void delete_book(Library *lib) {
    if (lib->count == 0) {
        printf(CLR_YELLOW "\n  [i] Library is empty.\n" CLR_RESET);
        return;
    }
    int acc = read_int("\n  Enter Accession Number to delete: ", 1, 999999);
    int idx = find_index_by_accession(lib, acc);
    if (idx == -1) {
        printf(CLR_RED "  [!] Book with Accession Number %d not found.\n" CLR_RESET, acc);
        return;
    }
    printf(CLR_GREEN "  [-] Removed \"%s\" (Acc. #%d) from library.\n" CLR_RESET, lib->records[idx].title, acc);
    for (size_t i = (size_t)idx; i + 1 < lib->count; i++) 
        lib->records[i] = lib->records[i + 1];
    lib->count--;
    lib_save(lib, DB_FILENAME);
}

void sort_and_display(Library *lib) {
    if (lib->count == 0) {
        printf(CLR_YELLOW "\n  [i] Library is empty.\n" CLR_RESET);
        return;
    }
    printf(CLR_CYAN "\n--- Sort Books ---\n" CLR_RESET);
    printf("  1. By Accession Number (Ascending)\n");
    printf("  2. By Title (A - Z)\n");
    printf("  3. By Price (Lowest to Highest)\n");
    int mode = read_int("  Choose sort order (1-3): ", 1, 3);
    if (mode == 1) 
        qsort(lib->records, lib->count, sizeof(Book), cmp_accession);
    else if (mode == 2) 
        qsort(lib->records, lib->count, sizeof(Book), cmp_title);
    else 
        qsort(lib->records, lib->count, sizeof(Book), cmp_price);
    lib_save(lib, DB_FILENAME);
    display_all_books(lib);
}

void library_analytics_and_fees(const Library *lib) {
    size_t issued_count = 0;
    double total_cost = 0.0;
    double available_value = 0.0;
    for (size_t i = 0; i < lib->count; i++) {
        total_cost += lib->records[i].price;
        if (lib->records[i].is_issued) 
            issued_count++;
        else 
            available_value += lib->records[i].price;
    }
    size_t in_library_count = lib->count - issued_count;
    double avg_price = (lib->count > 0) ? (total_cost / (double)lib->count) : 0.0;
    double security_deposit = avg_price * 1.5;
    double annual_fee = (lib->count > 0) ? (100.0 + (total_cost * 0.02)) : 0.0;
    printf(CLR_CYAN "\n================ LIBRARY INVENTORY & FINANCIAL REPORT ================\n" CLR_RESET);
    printf("  Total Books Registered          : " CLR_BOLD "%zu\n" CLR_RESET, lib->count);
    printf("  Books Currently in Library      : " CLR_GREEN "%zu\n" CLR_RESET, in_library_count);
    printf("  Books Currently Issued          : " CLR_YELLOW "%zu\n" CLR_RESET, issued_count);
    printf("  --------------------------------------------------------------------\n");
    printf("  Total Library Valuation         : %.2f\n", total_cost);
    printf("  In-Shelf Valuation              : %.2f\n", available_value);
    printf("  Average Book Price              : %.2f\n", avg_price);
    printf("  --------------------------------------------------------------------\n");
    printf("  Recommended Security Deposit    : " CLR_BOLD "%.2f" CLR_RESET " (1.5x avg book price)\n", security_deposit);
    printf("  Recommended Annual Joining Fee  : " CLR_BOLD "%.2f" CLR_RESET " (base + 2%% maintenance)\n", annual_fee);
    printf(CLR_CYAN "======================================================================\n" CLR_RESET);
}

int main(void) {
    Library lib;
    lib_init(&lib);
    lib_load(&lib, DB_FILENAME);
    printf(CLR_BOLD CLR_CYAN "=============================================\n");
    printf("     Welcome to Harsh's Library v2.0!        \n");
    printf("=============================================\n" CLR_RESET);
    printf("  [i] Loaded %zu record(s) from %s\n", lib.count, DB_FILENAME);
    while (1) {
        printf(CLR_BOLD "\nMain Menu:\n" CLR_RESET);
        printf("  1. Add book information\n");
        printf("  2. Display all books\n");
        printf("  3. List all books of a given author\n");
        printf("  4. Search for a specific book (Title / Accession No.)\n");
        printf("  5. Library inventory count, total cost & joining fee\n");
        printf("  6. Sort & list books (Accession No. / Title / Price)\n");
        printf("  7. Issue or Return a book\n");
        printf("  8. Delete a book record\n");
        printf("  9. Save & Exit\n");
        int choice = read_int("\nYour choice (1-9): ", 1, 9);
        switch (choice) {
            case 1:
                add_book(&lib);
                break;
            case 2:
                display_all_books(&lib);
                break;
            case 3:
                search_by_author(&lib);
                break;
            case 4:
                search_specific_book(&lib);
                break;
            case 5:
                library_analytics_and_fees(&lib);
                break;
            case 6:
                sort_and_display(&lib);
                break;
            case 7:
                toggle_issue_status(&lib);
                break;
            case 8:
                delete_book(&lib);
                break;
            case 9:
                lib_save(&lib, DB_FILENAME);
                lib_free(&lib);
                printf(CLR_GREEN "\nDatabase saved. Logging out of Library Management System!\n" CLR_RESET);
                return 0;
        }
    }
}
