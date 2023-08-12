/*
 * User space test program to exercise misc_example driver.
 *
 * (c) 2023 Chad Dupuis
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#define FILE_NAME       "/dev/misc_example"
#define BUF_SIZE        4096
#define STORE_SIZE      (100 * 1024 * 1024)

int main()
{
    int fd;
    int error;
    char write_buf[BUF_SIZE];
    char read_buf[BUF_SIZE];
    int i;
    int rand_offset;
    int rc = 0;

    /* Seed the pseudo random number generator*/
    srand(time(0));

    /* Write random data to the write buffer*/
    for (i = 0; i < BUF_SIZE; i++) {
        write_buf[i] = rand() % 256;
    }

    printf("Open file %s\n", FILE_NAME);
    fd = open(FILE_NAME, O_RDWR, 0);
    if (fd < 0) {
        perror("open() failed");
        return 1;
    }

    /* Seek to a random spot in the misc driver */
    rand_offset = rand() % STORE_SIZE;
    error = lseek(fd, rand_offset, SEEK_SET);
    if (error < 0) {
        perror("lseek");
        rc = 1;
        goto out;
    }

    /* Write random data to the misc driver */
    error = write(fd, write_buf, BUF_SIZE);
    if (error < 0) {
        perror("write");
        rc = 1;
        goto out;
    }

    /* Go back to the same spot we wrote to */
    error = lseek(fd, rand_offset, SEEK_SET);
    if (error < 0) {
        perror("lseek");
        rc = 1;
        goto out;
    }

    /* Zero out the read buffer */
    memset(read_buf, 0, BUF_SIZE);

    /* Read back the data we just wrote */
    error = read(fd, read_buf, BUF_SIZE);
    if (error < 0) {
        perror("read");
        rc = 1;
        goto out;
    }

    /* Compare the data read to what we wrote earlier */
    for (i = 0; i < BUF_SIZE; i++) {
        if (write_buf[i] != read_buf[i]) {
            printf("Miscompare at idx %d wb=%c rb=%c", i, write_buf[i], read_buf[i]);
            rc = 1;
            goto out;
        }
    }

    printf("Closing file %s\n", FILE_NAME);

out:
    close(fd);
    return rc;
}