/* Exercise the actual production callback unit with a controlled host backend. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../lib/liblwip/sysdepend/pico_rp2040/mqtt_utk.c"

static mqtt_client_t g_client = {false};
static uint32_t g_clock = 0U;
static uint32_t g_connects = 0U;
static uint32_t g_remote_calls = 0U;
static uint8_t g_remote_value = 255U;
static bool g_online = false;
static err_t g_connect_result = ERR_OK;
static err_t g_publish_result = ERR_OK;
static uint32_t g_timer_calls = 0U;
static uint32_t g_timer_delay = 0U;
static void (*g_timer_callback)(void *) = NULL;
static test_incoming_fn g_incoming = NULL;
static test_data_fn g_data = NULL;
static char g_last_topic[64];
static uint16_t g_last_length = 0U;
static uint8_t g_last_qos = 0U;
static uint8_t g_last_retain = 0U;
static uint8_t g_last_payload[26];
static struct mqtt_connect_client_info_t g_info;

mqtt_client_t *mqtt_client_new(void) { return &g_client; }
int mqtt_client_is_connected(mqtt_client_t *client) { return client->connected; }
u32_t sys_now(void) { return g_clock; }
int ipaddr_aton(char const *value, ip_addr_t *address)
{
    assert(0 == strcmp(value, "192.0.2.1"));
    address->address = 1U;
    return 1;
}
void sys_timeout(u32_t delay, void (*callback)(void *), void *context)
{
    assert(NULL == context);
    ++g_timer_calls;
    g_timer_delay = delay;
    g_timer_callback = callback;
}
err_t mqtt_client_connect(mqtt_client_t *client, ip_addr_t const *address,
                          u16_t port, test_connection_fn callback, void *context,
                          struct mqtt_connect_client_info_t const *info)
{
    assert(client == &g_client);
    assert(1U == address->address);
    assert(1883U == port);
    assert(NULL != callback && NULL == context);
    ++g_connects;
    g_info = *info;
    return g_connect_result;
}
void mqtt_set_inpub_callback(mqtt_client_t *client, test_incoming_fn incoming,
                             test_data_fn data, void *context)
{
    assert(client == &g_client && NULL == context);
    g_incoming = incoming;
    g_data = data;
}
err_t mqtt_subscribe(mqtt_client_t *client, char const *topic, u8_t qos,
                     test_request_fn callback, void *context)
{
    assert(client == &g_client && NULL == context && NULL != callback);
    assert(0 == strcmp(topic, "p5is20/car/cmd"));
    assert(1U == qos);
    return ERR_OK;
}
err_t mqtt_publish(mqtt_client_t *client, char const *topic, void const *payload,
                   u16_t length, u8_t qos, u8_t retain,
                   test_request_fn callback, void *context)
{
    assert(client == &g_client && NULL == context && NULL != callback);
    assert(sizeof(g_last_payload) >= length);
    assert(sizeof(g_last_topic) > strlen(topic));
    (void)strcpy(g_last_topic, topic);
    memcpy(g_last_payload, payload, length);
    g_last_length = length;
    g_last_qos = qos;
    g_last_retain = retain;
    return g_publish_result;
}
void robot_network(bool online) { g_online = online; }
void robot_remote(uint8_t command) { ++g_remote_calls; g_remote_value = command; }
size_t robot_telemetry(uint8_t *payload, size_t capacity)
{
    assert(26U == capacity);
    memset(payload, 0x5a, 26U);
    return 26U;
}

static void test_connection_and_timers(void)
{
    mqtt_utk_start();
    assert(1U == g_connects && g_b_connecting);
    assert(1U == g_timer_calls && 200U == g_timer_delay);
    assert(0 == strcmp(g_info.will_topic, "p5is20/car/status"));
    assert(0 == strcmp(g_info.will_msg, "offline"));
    assert(1U == g_info.will_qos && 1U == g_info.will_retain);
    mqtt_utk_start();
    assert(1U == g_connects && 1U == g_timer_calls);
    mqtt_connected(&g_client, NULL, MQTT_CONNECT_REFUSED);
    g_clock = 2999U;
    mqtt_utk_start();
    assert(1U == g_connects);
    g_clock = 3000U;
    mqtt_utk_start();
    assert(2U == g_connects);
    g_client.connected = true;
    mqtt_connected(&g_client, NULL, MQTT_CONNECT_ACCEPTED);
    assert(g_online && NULL != g_incoming && NULL != g_data);
    assert(0 == strcmp(g_last_topic, "p5is20/car/status"));
    assert(6U == g_last_length && 0 == memcmp(g_last_payload, "online", 6U));
    assert(1U == g_last_qos && 1U == g_last_retain);
    g_timer_callback(NULL);
    assert(0 == strcmp(g_last_topic, "p5is20/car/telemetry"));
    assert(26U == g_last_length && 0x5aU == g_last_payload[25]);
    assert(0U == g_last_qos && 0U == g_last_retain);
    assert(200U == g_timer_delay && 2U == g_timer_calls);
    g_publish_result = ERR_MEM;
    g_timer_callback(NULL);
    assert(!g_online);
    g_publish_result = ERR_OK;
    g_client.connected = false;
    g_timer_callback(NULL);
    assert(!g_online);
    /* Error retry deadline must remain correct over the 32-bit clock wrap. */
    g_clock = UINT32_MAX - 999U;
    mqtt_connected(&g_client, NULL, MQTT_CONNECT_REFUSED);
    assert(2000U == g_next_retry);
    g_clock = 1999U;
    mqtt_utk_start();
    assert(2U == g_connects);
    g_clock = 2000U;
    g_connect_result = ERR_MEM;
    mqtt_utk_start();
    assert(3U == g_connects && !g_b_connecting);
    assert(5000U == g_next_retry);
    g_clock = 4999U;
    mqtt_utk_start();
    assert(3U == g_connects);
    puts("MQTT callbacks: retry deadlines/wrap, last will, subscription, online status, telemetry timer and publish errors passed (stub backend)");
}

static void test_message_framing(void)
{
    uint32_t value = 0U;
    for (value = 0U; 256U > value; ++value)
    {
        uint8_t byte = (uint8_t)value;
        uint32_t calls = g_remote_calls;
        g_b_seen = false;
        g_incoming(NULL, "p5is20/car/cmd", 1U);
        g_data(NULL, &byte, 1U, MQTT_DATA_FLAG_LAST);
        assert(calls + 1U == g_remote_calls);
        assert((byte & 3U) == g_remote_value);
        g_incoming(NULL, "p5is20/car/cmd", 1U);
        g_data(NULL, &byte, 1U, MQTT_DATA_FLAG_LAST);
        assert(calls + 1U == g_remote_calls);
        g_incoming(NULL, "wrong/topic", 1U);
        g_data(NULL, &byte, 1U, MQTT_DATA_FLAG_LAST);
        assert(calls + 1U == g_remote_calls);
        g_b_seen = false;
        g_incoming(NULL, "p5is20/car/cmd", 2U);
        g_data(NULL, &byte, 1U, 0U);
        g_data(NULL, &byte, 1U, MQTT_DATA_FLAG_LAST);
        assert(calls + 1U == g_remote_calls);
        g_incoming(NULL, "p5is20/car/cmd", 1U);
        g_data(NULL, NULL, 0U, 0U);
        g_data(NULL, &byte, 1U, MQTT_DATA_FLAG_LAST);
        assert(calls + 1U == g_remote_calls);
        g_incoming(NULL, "p5is20/car/cmd", 1U);
        g_data(NULL, &byte, 1U, 0U);
        assert(calls + 1U == g_remote_calls); /* No dispatch before LAST. */
        g_data(NULL, NULL, 0U, MQTT_DATA_FLAG_LAST);
        assert(calls + 1U == g_remote_calls);
    }
    puts("MQTT framing: all 256 bytes plus duplicates, wrong topics, multi-byte and empty/unfinished callback sequences passed (stub backend)");
}

int main(void)
{
    test_connection_and_timers();
    test_message_framing();
    return 0;
}
