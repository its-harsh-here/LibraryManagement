# Library Management System (CLI v2.0)

A fast, crash-resistant, terminal-based Library Book Search and Management System written in standard C (`C11`).

What started in v1 as a basic `struct` array with `scanf` has been rebuilt into a persistent command-line tool. It handles multi-word book titles and author names, survives invalid keyboard inputs without infinite loops, scales dynamically on the heap, and automatically saves your catalog to a local binary database (`library.dat`).

---

## Features & Usability

- **Space-Safe Input:** Enter full book titles (e.g., *The C Programming Language*) and author names without breaking the input buffer.
- **Crash-Proof Menus:** Typing letters or symbols where numbers are expected won't trap the terminal in an infinite `scanf` loop.
- **Forgiving Search:** Case-insensitive substring matching lets you search `"knuth"` or `"art"` to find *Donald Knuth* or *The Art of Computer Programming*, alongside exact lookups by Accession Number.
- **Zero-Config Persistence:** Every time you add, issue, return, sort, or delete a book, changes are immediately committed to `library.dat` and reloaded automatically on the next startup.
- **Inventory & Financial Analytics:** Tracks total books vs. books currently on the shelf (`Total - Issued`), calculates total library valuation, and recommends a security deposit and annual joining fee.

---

## Quick Start

### 1. Compile
No third-party libraries are needed. Compile using `gcc` or `clang`:

```bash
gcc -Wall -Wextra -std=c11 -O2 library_v2.c -o library_v2
```

### 2. Run
```bash
./library_v2
```

### 3. Menu Options
1. **Add book information** — Prompts for Accession Number (checks for duplicates), Title, Author, Price, and Status.
2. **Display all books** — Prints a formatted ASCII table of the entire catalog.
3. **List all books of a given author** — Partial, case-insensitive author search.
4. **Search for a specific book** — Lookup by exact Accession Number or partial Title and view a detailed card.
5. **Library inventory count, total cost & joining fee** — Generates a live inventory and financial summary.
6. **Sort & list books** — Sorts in-place by Accession Number, Title (A–Z), or Price (Low–High).
7. **Issue or Return a book** — Flips the availability status of a book by Accession Number.
8. **Delete a book record** — Removes a book from the catalog and shifts remaining records.
9. **Save & Exit** — Flushes memory to disk, frees heap allocations, and exits cleanly.

---

## Data Structures Used

### 1. `Book` (Fixed-Width Record Struct)
Represents a single book entry. Fixed-size character arrays are used instead of heap pointers so the entire array can be serialized to and from disk in a single contiguous block read/write:

```c
typedef struct {
    int accession_num;       // Unique book ID
    char title[80];          // Book title
    char author[60];         // Author name
    double price;            // Book cost
    int is_issued;           // 0 = In Library, 1 = Issued
} Book;
```
* **Memory Footprint:** `160 bytes` per book on a standard 64-bit system (with 8-byte struct alignment).

### 2. `Library` (Dynamic Array Wrapper)
Replaces the fixed `rec[100]` stack array from v1 with a dynamically resizing heap array (similar to `std::vector` in C++):

```c
typedef struct {
    Book *records;           // Contiguous heap array of Book structs
    size_t count;            // Active number of books (N)
    size_t capacity;         // Allocated slots (starts at 16, doubles when full)
} Library;
```

---

## Core Code Logic

1. **Sanitized Input Layer (`read_string`, `read_int`, `read_double`)**
   Instead of raw `scanf()`, all input is read line-by-line using `fgets()`. If a user types more characters than the buffer holds, the remaining characters on `stdin` are flushed cleanly. Numeric inputs are parsed and range-validated via `strtol()` and `strtod()`.
2. **Geometric Heap Growth (`lib_ensure_capacity`)**
   When `count == capacity`, the program doubles the buffer capacity (`capacity *= 2`) via `realloc()`. Doubling ensures that adding books takes amortized `O(1)` memory allocation time.
3. **Overflow-Safe `qsort` Comparators (`cmp_accession`, `cmp_price`)**
   Instead of subtracting integers (`return a.n - b.n`), which can overflow on extreme values and copies whole structs by value, comparators read directly from `const Book *` pointers and use the branchless comparison idiom `(a > b) - (a < b)`.
4. **Portable Case-Insensitive Search (`contains_case_insensitive`)**
   Uses a sliding-window character comparison with `tolower()` from `<ctype.h>` so partial searches work identically across Linux, macOS, and Windows without relying on non-standard POSIX extensions like `strcasestr`.
5. **Binary File Serialization (`lib_save`, `lib_load`)**
   Writes a `size_t` header (`count`) to `library.dat` followed immediately by `count * sizeof(Book)` bytes via `fwrite()`. Loading reads the `count` header first, expands heap capacity if needed, and populates the array in a single `fread()` call.

---

## Time & Space Complexity

Let **N** be the number of books in the library, **C** be the current heap capacity, and **L** be the maximum string length of a title/author (`L <= 80`).

| Operation | Function | Time Complexity (Avg / Worst) | Auxiliary Space | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Load Database** | `lib_load()` | `Θ(N)` | `O(N)` | Allocates heap buffer and reads `N` contiguous records in one pass. |
| **Add Book** | `add_book()` | `Θ(N)` | `O(1)` amortized | Scans `N` books to prevent duplicate IDs; `realloc` doubling is `O(1)` amortized. |
| **Display All** | `display_all_books()` | `Θ(N)` | `O(1)` | Single linear traversal of the catalog. |
| **Search by ID** | `find_index_by_accession()` | `O(N)` | `O(1)` | Unordered linear scan across `records`. |
| **Search Author / Title** | `search_by_author()` | `Θ(N * L)` → `Θ(N)` | `O(1)` | Checks `N` books; string length `L` is bounded by constant `80`. |
| **Issue / Return** | `toggle_issue_status()` | `O(N)` | `O(1)` | Linear ID lookup + `O(1)` status toggle + sequential file save. |
| **Delete Book** | `delete_book()` | `O(N)` | `O(1)` | Finds book in `O(N)` and shifts remaining elements left by 1 index. |
| **Sort Catalog** | `sort_and_display()` | `O(N log N)` | `O(log N)` | Standard library `qsort` with `O(log N)` recursion stack frames. |
| **Analytics & Fees** | `library_analytics_and_fees()` | `Θ(N)` | `O(1)` | Single pass summing prices and counting availability flags. |
| **Save Database** | `lib_save()` | `Θ(N)` | `O(1)` | Sequential block write of `N * 160` bytes to disk. |

### Memory Complexity Summary
- **Total Heap Space:** `Θ(C * sizeof(Book))` where `N <= C <= 2N`, simplifying to **`Θ(N)`** (`~160 bytes` per book).
- **Total Stack Space:** **`O(1)`** during standard operations, peaking at **`O(log N)`** during `qsort`.

---

## Future Roadmap (v3.0 Ideas)
- Maintain records in sorted order or add an in-memory hash table for `O(1)` / `O(log N)` Accession ID lookups.
- Track borrower details, issue timestamps, and automated overdue fine calculations.
- Export catalog reports to CSV.
