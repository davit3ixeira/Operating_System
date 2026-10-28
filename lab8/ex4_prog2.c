#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define ARQUIVO "troca.dat"
#define MAGICO 0x4D454D31
#define VEZ_PROG1 1
#define VEZ_PROG2 2
#define VEZ_FIM 3

struct canal {
    int pronto;
    int vez;
    char texto[256];
};

int main (void)
{
    struct canal *c;
    struct stat sb;
    char recebido[256];
    int fd = -1;
    int vez;

    for (int t = 0; t < 100; t++) {
        fd = open(ARQUIVO, O_RDWR);
        if (fd != -1) {
            if (fstat(fd, &sb) == 0 && sb.st_size >= (off_t) sizeof(struct canal))
                break;
            close(fd);
            fd = -1;
        }
        usleep(100000);
    }

    if (fd == -1) {
        fprintf(stderr, "prog2: arquivo %s nao encontrado (execute o prog1 primeiro)\n", ARQUIVO);
        exit(1);
    }

    c = mmap(0, sizeof(struct canal), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (c == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    close(fd);

    while (__atomic_load_n(&c->pronto, __ATOMIC_ACQUIRE) != MAGICO)
        usleep(10000);

    printf("prog2 (pid %d): arquivo %s mapeado em %p\n", getpid(), ARQUIVO, (void *) c);
    fflush(stdout);

    for (;;) {
        while ((vez = __atomic_load_n(&c->vez, __ATOMIC_ACQUIRE)) == VEZ_PROG1)
            usleep(10000);

        if (vez == VEZ_FIM)
            break;

        strncpy(recebido, c->texto, sizeof recebido - 1);
        recebido[sizeof recebido - 1] = '\0';
        printf("prog2 leu:      \"%s\"\n", recebido);
        fflush(stdout);

        snprintf(c->texto, sizeof c->texto, "resposta do prog2 (pid %d) para [%.150s]", getpid(), recebido);
        printf("prog2 escreveu: \"%s\"\n", c->texto);
        fflush(stdout);
        __atomic_store_n(&c->vez, VEZ_PROG1, __ATOMIC_RELEASE);
    }

    printf("prog2: prog1 sinalizou o fim, encerrando\n");

    munmap(c, sizeof(struct canal));
    return 0;
}
