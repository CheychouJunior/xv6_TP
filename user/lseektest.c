#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

int
main(void)
{
    char buf[100];
    int fd;

    // Create and open a test file
    fd = open("lseektest.txt", O_RDWR | O_CREATE);
    if(fd < 0){
        printf("Error: Could not create test file\n");
        exit(1);
    }

    // Write test content
    if(write(fd, "Hello, world! This is a test file for lseek.", 44) < 0){
        printf("Error: Could not write to file\n");
        close(fd);
        exit(1);
    }
    // print the content of the file
    printf("\n\n# lseektest.txt file content\n");
    printf("Hello, world! This is a test file for lseek.\n\n");
    // Test 1: SEEK_SET - Move to specific position
    printf("Test 1: SEEK_SET\n");
    lseek(fd, 7, SEEK_SET);  // Move to 7th byte
    int n = read(fd, buf, 5);
    buf[n] = 0;  // Null terminate
    printf("Position after SEEK_SET 7: Read '%s'\n", buf);

    // Test 2: SEEK_CUR - Move from current position
    printf("Test 2: SEEK_CUR\n");
    lseek(fd, 2, SEEK_CUR);  // Move 2 bytes forward from current position
    n = read(fd, buf, 5);
    buf[n] = 0;
    printf("Position after SEEK_CUR 2: Read '%s'\n", buf);

    // Test 3: SEEK_END - Move relative to end of file
    printf("Test 3: SEEK_END\n");
    lseek(fd, -10, SEEK_END);  // Move 10 bytes before end of file
    n = read(fd, buf, 10);
    buf[n] = 0;
    printf("Position after SEEK_END -10: Read '%s'\n", buf);

    // Test 4: Return current position
    int pos = lseek(fd, 0, SEEK_CUR);
    printf("Current file position: %d\n", pos);

    // Additional error case tests
    printf("Test 5: Error cases\n");
    
    // Try seeking before start of file (should fail)
    if(lseek(fd, -50, SEEK_SET) < 0)
        printf("Correctly prevented seeking before file start\n");
    
    // Try seeking beyond file end (should fail)
    if(lseek(fd, 100, SEEK_SET) < 0)
        printf("Correctly prevented seeking beyond file end\n");

    close(fd);
    exit(0);
}
