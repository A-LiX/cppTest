#include <sys/mman.h>  // for shm_unlink
#include <iostream>

int main() {
    if (shm_unlink("/myqueue") == 0) {
        std::cout << "shm_unlink success\n";
    } else {
        perror("shm_unlink failed");
    }
}