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

// New GUI functions
int pwar_requires_restart(const pwar_config_t *old_config, const pwar_config_t *new_config);
int pwar_update_config(const pwar_config_t *config);
int pwar_init(const pwar_config_t *config);
int pwar_start(void);
int pwar_stop(void);
void pwar_cleanup(void);
int pwar_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBPWAR */
