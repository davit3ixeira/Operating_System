#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

static void mostraMapeamento(const char *rotulo, void *addr);

int main (int argc, char *argv[])
{
    struct stat sb;
    long pagesize = sysconf(_SC_PAGESIZE);
    long mapeado;
    size_t tamMap;
    char *p, *reserva, *alvo;
    const char *modo;
    int fd;

    if (argc < 3) {
        fprintf(stderr, "Uso: %s arquivo dentro|fora|alem|guarda\n", argv[0]);
        exit(1);
    }

    modo = argv[2];

    fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        exit(1);
    }

    mapeado = ((sb.st_size + pagesize - 1) / pagesize) * pagesize;
    tamMap = sb.st_size;
    if (strcmp(modo, "alem") == 0)
        tamMap = mapeado + pagesize;

    if (strcmp(modo, "guarda") == 0) {
        reserva = mmap(0, mapeado + pagesize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (reserva == MAP_FAILED) {
            perror("mmap reserva");
            exit(1);
        }
        p = mmap(reserva, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    } else {
        p = mmap(0, tamMap, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }

    if (p == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    printf("arquivo: %ld bytes | pedido ao mmap: %zu bytes | paginas mapeadas do arquivo: %zu\n",
           (long) sb.st_size, tamMap, (tamMap + pagesize - 1) / pagesize);

    alvo = strcmp(modo, "dentro") == 0 ? p + sb.st_size : p + mapeado;

    mostraMapeamento("mapeamento que contem o byte alvo", alvo);
    printf("escrevendo 'X' em p[%ld] (modo %s)\n", (long) (alvo - p), modo);
    fflush(stdout);

    *alvo = 'X';

    printf("escrita concluida sem erro\n");
    fflush(stdout);

    msync(p, sb.st_size, MS_SYNC);
    munmap(p, tamMap);
    close(fd);

    return 0;
}

static void mostraMapeamento(const char *rotulo, void *addr)
{
    FILE *f = fopen("/proc/self/maps", "r");
    char linha[512];
    unsigned long ini, fim;

    if (f == NULL)
        return;

    while (fgets(linha, sizeof linha, f) != NULL) {
        if (sscanf(linha, "%lx-%lx", &ini, &fim) == 2 && (unsigned long) addr >= ini && (unsigned long) addr < fim) {
            printf("%s: %s", rotulo, linha);
            fclose(f);
            return;
        }
    }

    printf("%s: nenhum mapeamento cobre o endereco %p\n", rotulo, addr);
    fclose(f);
}
