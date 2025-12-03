#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

extern  int alphasort(); 

int lista_e_soma(const char *path, int level) {
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

        // imprime com indentação de acordo com o nível
        printf("%*s[%s]\n", level * 2, "", entry->d_name);

        if (S_ISDIR(st.st_mode)) {
            // entra recursivamente no diretório
            total += lista_e_soma(fullpath, level + 1);
        } else if (S_ISREG(st.st_mode)) {
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

    printf("Diretório atual: %s\n\n", cwd);
    printf("[.]\n");  // raiz da listagem

    int total = lista_e_soma(cwd, 1);

    printf("\nSoma dos tamanhos de todos os arquivos: %lld bytes\n",
           (long long)total);

    return 0;
}
