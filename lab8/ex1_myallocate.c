#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

static char bufferSaida[8192];
static void *memorypool;

void *myallocate(int n);
void mydeallocate(void *bloco);
void initmemorypool(void);
void resetmemorypool(void);
void testeOrdemInversa(char *a, char *c, int tamC);

int main (void)
{
    setvbuf(stdout, bufferSaida, _IOFBF, sizeof bufferSaida);

    initmemorypool();
    printf("pool iniciado: break inicial = %p\n\n", memorypool);

    int *a = (int *) myallocate(10 * sizeof(int));
    printf("a = myallocate(%zu)   -> %p | break = %p\n", 10 * sizeof(int), (void *) a, sbrk(0));

    char *b = (char *) myallocate(64);
    printf("b = myallocate(64)   -> %p | break = %p\n", (void *) b, sbrk(0));

    char *c = (char *) myallocate(8192);
    printf("c = myallocate(8192) -> %p | break = %p\n\n", (void *) c, sbrk(0));

    for (int i = 0; i < 10; i++)
        a[i] = i * i;
    strcpy(b, "memoria obtida direto do heap com sbrk()");
    memset(c, 'Z', 8192);

    printf("a[0..9] =");
    for (int i = 0; i < 10; i++)
        printf(" %d", a[i]);
    printf("\nb = \"%s\"\n", b);
    printf("c[0] = '%c', c[8191] = '%c'\n", c[0], c[8191]);
    printf("b - a = %ld bytes, c - b = %ld bytes (blocos contiguos)\n\n", (long) (b - (char *) a), (long) (c - b));
    fflush(stdout);

    testeOrdemInversa((char *) a, c, 8192);

    mydeallocate(c);
    printf("mydeallocate(c)   -> break = %p (igual a c: %s)\n", sbrk(0), sbrk(0) == (void *) c ? "sim" : "nao");
    mydeallocate(b);
    printf("mydeallocate(b)   -> break = %p (igual a b: %s)\n", sbrk(0), sbrk(0) == (void *) b ? "sim" : "nao");
    resetmemorypool();
    printf("resetmemorypool() -> break = %p (igual ao inicio do pool: %s)\n", sbrk(0), sbrk(0) == memorypool ? "sim" : "nao");

    return 0;
}

void *myallocate(int n)
{
    void *bloco = sbrk(n);
    return bloco == (void *) -1 ? NULL : bloco;
}

void mydeallocate(void *bloco)
{
    brk(bloco);
}

void initmemorypool(void)
{
    memorypool = sbrk(0);
}

void resetmemorypool(void)
{
    brk(memorypool);
}

void testeOrdemInversa(char *a, char *c, int tamC)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        printf("filho: liberando 'a' antes de 'b' e 'c' com brk(a)\n");
        fflush(stdout);
        mydeallocate(a);
        printf("filho: break agora em %p, lendo c[%d]...\n", sbrk(0), tamC - 1);
        fflush(stdout);
        volatile char x = c[tamC - 1];
        printf("filho: leitura funcionou (%c)\n", x);
        fflush(stdout);
        _exit(0);
    }

    waitpid(pid, &status, 0);
    if (WIFSIGNALED(status))
        printf("pai: filho terminou pelo sinal %d (%s)\n\n", WTERMSIG(status), WTERMSIG(status) == SIGSEGV ? "SIGSEGV" : "outro");
    else
        printf("pai: filho terminou normalmente com codigo %d\n\n", WEXITSTATUS(status));
    fflush(stdout);
}
