#include <elite_api.h>
#include <cstring>
int main() { return std::strcmp(elite_version_string(), "1.1.0") != 0; }
