#ifndef FINORA_SDK_H
#define FINORA_SDK_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct finora_ctx_t finora_ctx_t;

/**
 * Finora bağlantı oturumu başlatır ve PQC el sıkışmasını tamamlar.
 */
finora_ctx_t* finora_client_connect(const char* remote_ip, 
                                    uint16_t port, 
                                    const char* config_path);

/**
 * Sıfır kopyalama (zero-copy) tampon üzerinden şifreli finansal veri iletir.
 * Halka arabellekten doğrudan sokete yazar.
 */
int finora_send(finora_ctx_t* ctx, 
                const uint8_t* payload, 
                size_t length, 
                uint8_t payload_type);

/**
 * Şifresi çözülmüş ham veriyi okur.
 */
ssize_t finora_recv(finora_ctx_t* ctx, 
                    uint8_t* buffer, 
                    size_t max_length);

/**
 * Oturumu güvenli bir şekilde kapatır ve oturum anahtarlarını bellekten siler.
 */
void finora_disconnect(finora_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif // FINORA_SDK_H
