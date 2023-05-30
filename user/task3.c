#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define PAGE_SIZE 4096 

int main() {
    int i;
    char* pointers[32-4];
    getpid();

    // Allocate and access user pages
    for (i = 0; i < 32-4; i++) {
        // Allocate a user page
        pointers[i] = sbrk(PAGE_SIZE);
        if (pointers[i] == (char*)-1) {
            printf("Error: Out of memory\n");
            exit(1);
        }

        // Access the page to ensure it's loaded into RAM
        pointers[i] = "x";

        // Print the page number and its content
        printf("Page %d: %s\n", i+4, pointers[i]);
    }
    getpid();

    int page_accesses[] = {5, 7, 16, 25, 1, 4, 26, 15, 12, 0, 28, 7, 26, 17, 8, 7, 5, 9, 0, 2};

    for (int i = 0; i < 20; i++)
    {
        printf("accessing page %d\n",page_accesses[i]);
        pointers[page_accesses[i]] = "a";
    }
    getpid();

    exit(0);
}

