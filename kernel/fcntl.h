#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400

#define SEEK_SET 0 // from beginning of file
#define SEEK_CUR 1 // from current position
#define SEEK_END 2 // from end of file

// Define permission bits
#define S_IRUSR 0400  // Read permission for owner
#define S_IWUSR 0200  // Write permission for owner
#define S_IXUSR 0100  // Execute permission for owner