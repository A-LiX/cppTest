#include <sys/mman.h>  // for shm_unlink
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " /shm_name" << std::endl;
        return 1;
    }

    const char* shm_name = argv[1];
    if (shm_unlink(shm_name) == 0) {
        std::cout << "shm_unlink success: " << shm_name << std::endl;
    } else {
        perror("shm_unlink failed");
        return 1;
    }

    return 0;
}