#include <sys/types.h> 
#include <sys/dir.h> 
#include <sys/param.h>
#include <unistd.h> // Para getwd()
#include <stdlib.h> // Para exit()
#include <sys/stat.h> // Para stat()
#include <time.h> // para time()
#include <string.h>
#include <stdio.h>

#define FALSE 0 
#define TRUE 1 

extern  int alphasort(); 
char pathname[MAXPATHLEN];

 
int file_select(const struct dirent *entry) {
    if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0)) {
        return FALSE; 
    }
    
    else {
        return TRUE; 
    }
} 

main(void)  {  
    int count, i;

    struct dirent **files;
    struct stat buf;

    char fileName[256];
    char fileAbsPath[1024];
    char barra[2] = "/";

    if (getwd(pathname) == NULL )   {
        printf("Error getting path\n"); exit(0); 
    } 
    
    printf("Current Working Directory = %s\n",pathname); 
    count = scandir( pathname, &files, file_select, alphasort); 
    
    /* If no files found, make a non-selectable menu item */
    if (count <= 0) {
        printf("No files in this directory\n"); 
        exit(0); 
    }
    
    printf("Number of files = %d\n\n",count);

    for (i=0; i<count; i++) {
        strcpy(fileAbsPath, pathname);
        strcpy(fileName, files[i]->d_name);

        strcat(fileAbsPath, barra);
        strcat(fileAbsPath, fileName);

        stat(fileAbsPath, &buf);

        time_t agora = time(NULL);

        // diferença em segundos (agora - última modificação)
        double diff_seg = difftime(agora, buf.st_mtime);

        // converte para dias
        double idade_dias = diff_seg / (60 * 60 * 24);

        printf("\n\nInformacoes sobre o arquivo: %s:\n", fileName);

        printf("Numero do inode: %d", buf.st_ino);
        printf(", Tamanho em bytes: %d", buf.st_size);
        printf(", Idade em dias: %.2f\n\n", idade_dias);
        printf(", Numero de links simbolicos: %d", buf.st_nlink);
        
        printf("\n"); /* flush buffer */
    }
} 