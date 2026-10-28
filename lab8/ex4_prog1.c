#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define ARQUIVO "troca.dat"
#define RODADAS 5
#define MAGICO 0x4D454D31
#define VEZ_PROG1 1
#define VEZ_PROG2 2
#define VEZ_FIM 3

struct canal {
    int pronto;
    int vez;
    char texto[256];
};

static void esperaVez(struct canal *c, int quem);

int main (void)
{
    struct canal *c;
    int fd;

    unlink(ARQUIVO);

    fd = open(ARQUIVO, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    if (ftruncate(fd, sizeof(struct canal)) == -1) {
        perror("ftruncate");
        exit(1);
    }

    c = mmap(0, sizeof(struct canal), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (c == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    close(fd);

    __atomic_store_n(&c->vez, VEZ_PROG1, __ATOMIC_RELEASE);
    __atomic_store_n(&c->pronto, MAGICO, __ATOMIC_RELEASE);

    printf("prog1 (pid %d): arquivo %s criado e mapeado em %p\n", getpid(), ARQUIVO, (void *) c);
    fflush(stdout);

    for (int r = 1; r <= RODADAS; r++) {
        esperaVez(c, VEZ_PROG1);
        snprintf(c->texto, sizeof c->texto, "rodada %d: mensagem do prog1 (pid %d)", r, getpid());
        printf("prog1 escreveu: \"%s\"\n", c->texto);
        fflush(stdout);
        __atomic_store_n(&c->vez, VEZ_PROG2, __ATOMIC_RELEASE);

        esperaVez(c, VEZ_PROG1);
        printf("prog1 leu:      \"%s\"\n", c->texto);
        fflush(stdout);
    }

    __atomic_store_n(&c->vez, VEZ_FIM, __ATOMIC_RELEASE);
    printf("prog1: %d rodadas concluidas, encerrando\n", RODADAS);

    munmap(c, sizeof(struct canal));
    return 0;
}

static void esperaVez(struct canal *c, int quem)
{
    while (__atomic_load_n(&c->vez, __ATOMIC_ACQUIRE) != quem)
        usleep(10000);
}
