#include <elite_api.h>
#include <string.h>
int main(void) { return strcmp(elite_version_string(), "1.1.0") != 0 || elite_platform_admit().status != ELITE_OK; }
