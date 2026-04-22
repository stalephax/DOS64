#include <iostream>
struct MemorySystem {
    static const unsigned long long MAX_PAGES = 256 * 256;
    unsigned char bitmap[MAX_PAGES / 8];
    unsigned long long total_pages = 0;
    unsigned long long free_pages  = 0;
};
int main() {
    std::cout << "sizeof(MemorySystem) = " << sizeof(MemorySystem) << " octets\n";
    std::cout << "soit " << sizeof(MemorySystem)/1024 << " Ko\n";
}
