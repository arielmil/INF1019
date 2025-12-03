#include <sys/types.h>
#include <dirent.h>     // scandir, struct dirent
#include <sys/stat.h>   // stat
#include <unistd.h>     // getcwd
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

extern  int alphasort();

// Percorre recursivamente "path" e devolve a soma dos tamanhos de todos os arquivos regulares
int soma_tamanhos(const char *path) {
    struct dirent **namelist;
    int n;
    int total = 0;

    n = scandir(path, &namelist, NULL, alphasort);
    if (n < 0) {
        perror("scandir");
        return 0;
    }

    while (n--) {
        struct dirent *entry = namelist[n];

        // ignora . e ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            free(entry);
            continue;
        }

        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) == -1) {
            perror("stat");
            free(entry);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // se for diretório, soma recursivamente
            total += soma_tamanhos(fullpath);
        } else if (S_ISREG(st.st_mode)) {
            // se for arquivo regular, soma o tamanho
            total += st.st_size;
        }

        free(entry);
    }

    free(namelist);
    return total;
}

int main(void) {
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }

    printf("Diretório atual: %s\n", cwd);

    int total = soma_tamanhos(cwd);

    printf("Soma dos tamanhos de todos os arquivos: %lld bytes\n",
           (long long)total);

    return 0;
}