#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string_view>
#include <optional>
#include "Guard.h"
#include <iostream>
#include <ostream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char** argv)
{

    const char* fin = 0;

    if (argc > 2)
        return fprintf(stderr, "Guard Checker\n\tUsage: %s somefile.wasm\n", argv[0]);
    else if (argc == 1)
        fin = "-";
    else
        fin = argv[1];

    int fd = 0;
    if (strcmp(fin, "-") != 0)
        fd = open(fin, O_RDONLY);

    if (fd < 0)
        return fprintf(stderr, "Could not open file for reading:`%s`\n", fin);

    off_t len = fd == 0 ? 0 :
        lseek(fd, 0, SEEK_END);

    if (fd != 0)
        lseek(fd, 0, SEEK_SET);

    int length_known = len > 0;

    uint8_t hook_data[0x100000U];

    uint8_t* ptr = hook_data;

    size_t upto = 0;

    fcntl(fd, F_SETFL, fcntl(0, F_GETFL) & ~O_NONBLOCK);

    while (ptr - hook_data < sizeof(hook_data))
    {
        if (length_known)
        {
            if (upto >= len)
                break;
        }
        else
            len = upto + 1;
        
        size_t bytes_read = read(fd, ptr + upto, len - upto);
        
        if (!length_known && bytes_read == 0)
            break;

        if (bytes_read < 0)
            return fprintf(stderr, "Error reading file `%s`, only %ld bytes could be read\n", fin, upto);

        upto += bytes_read;
    }

    std::vector<uint8_t> hook(upto);
    memcpy(hook.data(), hook_data, upto);

    printf("Read %ld bytes from `%s` successfully...\n", upto, fin);

    close(fd);

    auto result = 
        validateGuards(hook, std::cout, "");

    if (!result)
    {
        printf("Hook validation failed.\n");
        return 1;
    }

    printf("\nHook validation successful!\n");

    return 0;
}
