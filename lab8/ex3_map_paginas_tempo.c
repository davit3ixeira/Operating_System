#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define REPETICOES 5
#define SAIDA_WRITE "saida_write.bin"
#define SAIDA_MMAP "saida_mmap.bin"

typedef struct {
    double semSync;
    double comSync;
} Tempos;

typedef struct {
    const char *nome;
    int usaMmap;
    size_t bloco;
} Cenario;

static long agoraUs(void);
static long paginasNoKernel(void *p, long pagesize);
static Tempos escreveWrite(const char *src, off_t n, size_t bloco);
static Tempos escreveMmap(const char *src, off_t n, size_t bloco);
static void mede(const char *src, off_t n, long pagesize);

int main (int argc, char *argv[])
{
    struct stat sb;
    long pagesize, paginas, noKernel;
    char *p;
    int fd;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <file> [medir]\n", argv[0]);
        exit(1);
    }

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }

    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        exit(1);
    }

    if (!S_ISREG(sb.st_mode)) {
        fprintf(stderr, "%s is not a file\n", argv[1]);
        exit(1);
    }

    p = mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    if (close(fd) == -1) {
        perror("close");
        exit(1);
    }

    pagesize = sysconf(_SC_PAGESIZE);
    paginas = (sb.st_size + pagesize - 1) / pagesize;
    noKernel = paginasNoKernel(p, pagesize);

    printf("arquivo: %s\n", argv[1]);
    printf("tamanho do arquivo: %ld bytes\n", (long) sb.st_size);
    printf("tamanho da pagina: %ld bytes\n", pagesize);
    printf("paginas necessarias: %ld\n", paginas);
    printf("bytes mapeados (paginas x tamanho): %ld\n", paginas * pagesize);
    printf("bytes sobrando na ultima pagina (preenchidos com zero): %ld\n", paginas * pagesize - (long) sb.st_size);
    printf("paginas do mapeamento segundo /proc/self/maps: %ld\n", noKernel);

    if (argc >= 3 && strcmp(argv[2], "medir") == 0)
        mede(p, sb.st_size, pagesize);

    if (munmap(p, sb.st_size) == -1) {
        perror("munmap");
        exit(1);
    }

    return 0;
}

static long agoraUs(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

static long paginasNoKernel(void *p, long pagesize)
{
    FILE *f = fopen("/proc/self/maps", "r");
    char linha[512];
    unsigned long ini, fim;

    if (f == NULL)
        return -1;

    while (fgets(linha, sizeof linha, f) != NULL) {
        if (sscanf(linha, "%lx-%lx", &ini, &fim) == 2 && ini == (unsigned long) p) {
            fclose(f);
            return (long) ((fim - ini) / pagesize);
        }
    }

    fclose(f);
    return -1;
}

static Tempos escreveWrite(const char *src, off_t n, size_t bloco)
{
    Tempos t;
    long ini, meio, fim;
    off_t pos;
    size_t q;
    int fd;

    ini = agoraUs();

    fd = open(SAIDA_WRITE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open saida");
        exit(1);
    }

    for (pos = 0; pos < n; pos += bloco) {
        q = (n - pos < (off_t) bloco) ? (size_t) (n - pos) : bloco;
        if (write(fd, src + pos, q) != (ssize_t) q) {
            perror("write");
            exit(1);
        }
    }

    meio = agoraUs();

    if (fsync(fd) == -1) {
        perror("fsync");
        exit(1);
    }

    fim = agoraUs();
    close(fd);

    t.semSync = (double) (meio - ini);
    t.comSync = (double) (fim - ini);
    return t;
}

static Tempos escreveMmap(const char *src, off_t n, size_t bloco)
{
    Tempos t;
    long ini, meio, fim;
    off_t pos;
    size_t q;
    char *d;
    volatile char *v;
    int fd;

    ini = agoraUs();

    fd = open(SAIDA_MMAP, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open saida");
        exit(1);
    }

    if (ftruncate(fd, n) == -1) {
        perror("ftruncate");
        exit(1);
    }

    d = mmap(0, n, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (d == MAP_FAILED) {
        perror("mmap saida");
        exit(1);
    }

    if (bloco == 1) {
        v = d;
        for (pos = 0; pos < n; pos++)
            v[pos] = src[pos];
    } else {
        for (pos = 0; pos < n; pos += bloco) {
            q = (n - pos < (off_t) bloco) ? (size_t) (n - pos) : bloco;
            memcpy(d + pos, src + pos, q);
        }
    }

    meio = agoraUs();

    if (msync(d, n, MS_SYNC) == -1) {
        perror("msync");
        exit(1);
    }

    fim = agoraUs();
    munmap(d, n);
    close(fd);

    t.semSync = (double) (meio - ini);
    t.comSync = (double) (fim - ini);
    return t;
}

static void mede(const char *src, off_t n, long pagesize)
{
    Cenario cenarios[4] = {
        {"write(), 1 byte por chamada", 0, 1},
        {"write(), 1 pagina por chamada", 0, 0},
        {"mmap, escrita byte a byte", 1, 1},
        {"mmap, memcpy de 1 pagina por vez", 1, 0}
    };
    double semSync[4], comSync[4];
    volatile unsigned long soma = 0;
    off_t i;
    int c, r;

    for (i = 0; i < n; i++)
        soma += (unsigned char) src[i];

    printf("\nescrita de %ld bytes, media de %d repeticoes (tempos em microssegundos)\n", (long) n, REPETICOES);
    printf("%-36s %16s %16s\n", "cenario", "sem sync", "com fsync/msync");

    for (c = 0; c < 4; c++) {
        size_t bloco = cenarios[c].bloco == 0 ? (size_t) pagesize : cenarios[c].bloco;
        double somaSem = 0, somaCom = 0;

        for (r = 0; r < REPETICOES; r++) {
            Tempos t = cenarios[c].usaMmap ? escreveMmap(src, n, bloco) : escreveWrite(src, n, bloco);
            somaSem += t.semSync;
            somaCom += t.comSync;
        }

        semSync[c] = somaSem / REPETICOES;
        comSync[c] = somaCom / REPETICOES;
        printf("%-36s %16.0f %16.0f\n", cenarios[c].nome, semSync[c], comSync[c]);
    }

    printf("\nmmap byte a byte comparado com write() de 1 byte:  %.1f vezes mais rapido (sem sync), %.1f vezes (com sync)\n",
           semSync[0] / semSync[2], comSync[0] / comSync[2]);
    printf("mmap memcpy comparado com write() de 1 pagina:     %.1f vezes mais rapido (sem sync), %.1f vezes (com sync)\n",
           semSync[1] / semSync[3], comSync[1] / comSync[3]);
}
