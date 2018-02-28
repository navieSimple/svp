#include <guest_channel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

char *buf;
int channel;

void tst_read(int n, int size)
{
    clock_t start = clock();
    int sn = n;
    while (n--)
        svp_channel_read(channel, buf, size);
    clock_t finish = clock();
    printf("read test: size %u mb time %.3lf s\n",
           sn * (size / (1024 * 1024)), (double)(finish - start) / CLOCKS_PER_SEC);
}

void tst_write(int n, int size)
{
    clock_t start = clock();
    int sn = n;
    while (n--)
        svp_channel_write(channel, buf, size);
    clock_t finish = clock();
    printf("write test: size %u mb time %.3lf s\n",
           sn * (size / (1024 * 1024)), (double)(finish - start) / CLOCKS_PER_SEC);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("usage:\n"
               "    tst_speed [NUM] [SIZE_IN_MB]\n");
        exit(-1);
    }
    int n = atoi(argv[1]);
    int mb = atoi(argv[2]);
    printf("%d %d\n", n, mb * 1024 * 1024);
    channel = svp_channel_open("/svp/nop", 0);
    buf = _aligned_malloc(8, 32 * 1024 * 1024);
    tst_read(n, mb * 1024 * 1024);
    tst_write(n, mb * 1024 * 1024);
    svp_channel_close(channel);
    free(buf);
    return 0;
}
