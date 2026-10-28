#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

int main (int argc, char *argv[])
{
    struct stat sb;
    size_t antigo, novo, tamTexto;
    int vezes = 1;
    char *p;
    int fd;

    if (argc < 3) {
        fprintf(stderr, "Uso: %s arquivo texto [vezes]\n", argv[0]);
        exit(1);
    }

    if (argc >= 4)
        vezes = atoi(argv[3]);

    tamTexto = strlen(argv[2]);

    fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        exit(1);
    }

    if (!S_ISREG(sb.st_mode) || sb.st_size == 0) {
        fprintf(stderr, "%s precisa ser um arquivo regular nao vazio\n", argv[1]);
        exit(1);
    }

    antigo = sb.st_size;
    novo = antigo + tamTexto * vezes;

    p = mmap(0, antigo, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    printf("mapeamento inicial: %p com %zu bytes\n", (void *) p, antigo);

    if (ftruncate(fd, novo) == -1) {
        perror("ftruncate");
        exit(1);
    }

    if (munmap(p, antigo) == -1) {
        perror("munmap");
        exit(1);
    }

    p = mmap(0, novo, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap novo");
        exit(1);
    }

    printf("novo mapeamento:    %p com %zu bytes\n", (void *) p, novo);

    for (int i = 0; i < vezes; i++)
        memcpy(p + antigo + (size_t) i * tamTexto, argv[2], tamTexto);

    if (msync(p, novo, MS_SYNC) == -1) {
        perror("msync");
        exit(1);
    }

    munmap(p, novo);
    close(fd);

    printf("arquivo passou de %zu para %zu bytes\n", antigo, novo);

    return 0;
}
