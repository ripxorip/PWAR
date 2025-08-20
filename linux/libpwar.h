#ifndef LIBPWAR
#define LIBPWAR

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PWAR_MAX_IP_LEN 64

typedef struct {
    char stream_ip[PWAR_MAX_IP_LEN];
    int stream_port;
    int passthrough_test;
    int oneshot_mode;
    int buffer_size;
} pwar_config_t;

int pwar_cli_run(const pwar_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* LIBPWAR */
