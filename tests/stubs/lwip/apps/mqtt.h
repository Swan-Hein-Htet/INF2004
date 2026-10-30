/* Host-only stand-ins; this is not a network stack or hardware simulator. */
#ifndef TEST_MQTT_STUB_H
#define TEST_MQTT_STUB_H
#include <stdbool.h>
#include <stdint.h>
typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef int err_t;
typedef struct { bool connected; } mqtt_client_t;
typedef struct { uint32_t address; } ip_addr_t;
typedef enum { MQTT_CONNECT_ACCEPTED = 0, MQTT_CONNECT_REFUSED = 3 } mqtt_connection_status_t;
#define ERR_OK 0
#define ERR_MEM (-1)
#define MQTT_DATA_FLAG_LAST 1U
struct mqtt_connect_client_info_t {
    char const *client_id;
    u16_t keep_alive;
    char const *will_topic;
    char const *will_msg;
    u8_t will_qos;
    u8_t will_retain;
};
typedef void (*test_request_fn)(void *, err_t);
typedef void (*test_connection_fn)(mqtt_client_t *, void *, mqtt_connection_status_t);
typedef void (*test_incoming_fn)(void *, char const *, u32_t);
typedef void (*test_data_fn)(void *, u8_t const *, u16_t, u8_t);
mqtt_client_t *mqtt_client_new(void);
int mqtt_client_is_connected(mqtt_client_t *client);
err_t mqtt_client_connect(mqtt_client_t *client, ip_addr_t const *address,
                          u16_t port, test_connection_fn callback, void *context,
                          struct mqtt_connect_client_info_t const *info);
void mqtt_set_inpub_callback(mqtt_client_t *client, test_incoming_fn incoming,
                             test_data_fn data, void *context);
err_t mqtt_subscribe(mqtt_client_t *client, char const *topic, u8_t qos,
                     test_request_fn callback, void *context);
err_t mqtt_publish(mqtt_client_t *client, char const *topic, void const *payload,
                   u16_t length, u8_t qos, u8_t retain,
                   test_request_fn callback, void *context);
int ipaddr_aton(char const *text, ip_addr_t *address);
u32_t sys_now(void);
void sys_timeout(u32_t delay, void (*callback)(void *), void *context);
#endif
