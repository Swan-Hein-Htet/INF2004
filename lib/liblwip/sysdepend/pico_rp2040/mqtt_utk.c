/* MQTT callbacks execute solely on the template's CYW43 owner task. */
#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"
#include "lwip/timeouts.h"
#include "lwip/sys.h"
#include "wifi_secrets.h"
#include "mqtt_utk.h"
#include "../../../../app_program/board.h"
#include <string.h>

#define MQTT_TELEMETRY "p5is20/car/telemetry"
#define MQTT_COMMAND "p5is20/car/cmd"
#define MQTT_STATUS "p5is20/car/status"

static mqtt_client_t *g_p_client = NULL;
static bool g_b_connecting = false;
static bool g_b_timer_started = false;
static bool g_b_rx_valid = false;
static uint8_t g_rx_byte = 0U;
static uint16_t g_rx_count = 0U;
static uint8_t g_sequence = 0U;
static bool g_b_seen = false;
static uint32_t g_next_retry = 0U;

static void mqtt_incoming(void *p_context, char const *p_topic, u32_t total)
{
    (void)p_context;
    g_b_rx_valid = (1U == total) && (0 == strcmp(p_topic, MQTT_COMMAND));
    g_rx_count = 0U;
}

static void mqtt_data(void *p_context, u8_t const *p_data, u16_t length,
                      u8_t flags)
{
    uint8_t command = 0U;
    (void)p_context;
    if ((1U == length) && (0U == g_rx_count))
    {
        g_rx_byte = p_data[0];
    }
    else
    {
        g_b_rx_valid = false;
    }
    g_rx_count = (uint16_t)(g_rx_count + length);
    if (0U != (flags & MQTT_DATA_FLAG_LAST))
    {
        if (g_b_rx_valid && robot_command(&g_rx_byte, g_rx_count, &g_sequence,
                                          &g_b_seen, &command))
        {
            robot_remote(command);
        }
        g_b_rx_valid = false;
    }
}

static void mqtt_publish_done(void *p_context, err_t result)
{
    (void)p_context;
    if (ERR_OK != result)
    {
        robot_network(false);
    }
}

static void mqtt_tick(void *p_context)
{
    uint8_t payload[ROBOT_TELEMETRY_BYTES] = {0U};
    (void)p_context;
    if ((NULL != g_p_client) && mqtt_client_is_connected(g_p_client))
    {
        size_t length = robot_telemetry(payload, sizeof(payload));
        err_t result =
            mqtt_publish(g_p_client, MQTT_TELEMETRY, payload, (u16_t)length, 0U,
                         0U, mqtt_publish_done, NULL);
        robot_network(ERR_OK == result);
    }
    else
    {
        robot_network(false);
    }
    sys_timeout(200U, mqtt_tick, NULL);
}

static void mqtt_connected(mqtt_client_t *p_client, void *p_context,
                           mqtt_connection_status_t status)
{
    (void)p_context;
    g_b_connecting = false;
    if (MQTT_CONNECT_ACCEPTED == status)
    {
        mqtt_set_inpub_callback(p_client, mqtt_incoming, mqtt_data, NULL);
        (void)mqtt_subscribe(p_client, MQTT_COMMAND, 1U, mqtt_publish_done,
                             NULL);
        (void)mqtt_publish(p_client, MQTT_STATUS, "online", 6U, 1U, 1U,
                           mqtt_publish_done, NULL);
        robot_network(true);
    }
    else
    {
        g_next_retry = sys_now() + 3000U;
        robot_network(false);
    }
}

void mqtt_utk_start(void)
{
    struct mqtt_connect_client_info_t information = {0};
    ip_addr_t address = {0};
    err_t result = ERR_OK;
    if (!g_b_timer_started)
    {
        sys_timeout(200U, mqtt_tick, NULL);
        g_b_timer_started = true;
    }
    if (g_b_connecting ||
        ((NULL != g_p_client) && mqtt_client_is_connected(g_p_client)) ||
        (0 > (int32_t)(sys_now() - g_next_retry)))
    {
        return;
    }
    if (NULL == g_p_client)
    {
        g_p_client = mqtt_client_new();
    }
    if ((NULL == g_p_client) || (!ipaddr_aton(MQTT_BROKER_IP, &address)))
    {
        return;
    }
    information.client_id = MQTT_CLIENT_ID;
    information.keep_alive = 15U;
    information.will_topic = MQTT_STATUS;
    information.will_msg = "offline";
    information.will_qos = 1U;
    information.will_retain = 1U;
    g_b_connecting = true;
    result = mqtt_client_connect(g_p_client, &address, MQTT_BROKER_PORT,
                                 mqtt_connected, NULL, &information);
    if (ERR_OK != result)
    {
        g_b_connecting = false;
        g_next_retry = sys_now() + 3000U;
        robot_network(false);
    }
}
