#include <guest_channel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char buf[65536];

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("usage:\n"
               "    tst_hostcmd [HOST_CMD]\n");
        exit(-1);
    }
    int channel = svp_channel_open("/svp/hostcmd", 0);
    int n;
    fprintf(stderr, "channel open %d\n", channel);
    if (channel == -1)
        exit(-1);
    char s[] = "aBcDe";
    n = svp_channel_ioctl(channel, 1, s, strlen(s) + 1);
    fprintf(stderr, "ioctl ret %d result %s\n", n, s);
    sleep(1);
    n = svp_channel_write(channel, argv[1], strlen(argv[1]) + 1);
    fprintf(stderr, "write to channel length %d ret %d\n", strlen(argv[1]) + 1, n);
    n = svp_channel_read(channel, buf, sizeof(buf));
    fprintf(stderr, "read from channel ret %d\n", n);
    if (n > 0)
        fprintf(stderr, "%s", buf);
    svp_channel_close(channel);
    return 0;
}
