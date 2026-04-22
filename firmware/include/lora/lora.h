/**
 * @file lora.h
 *
 * @brief
 *
 * @date 3/24/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LORA_H
#define ARES_LORA_H

#include <lora/packet.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>

/**
 * @enum lora_transport_evt
 * LoRa transport events.
 */
enum lora_transport_evt {
    LORA_TRANSPORT_EVT_RX_RDY, ///< LoRa Rx data ready.
    LORA_TRANSPORT_EVT_TX_RDY, ///< LoRa Tx ready
};

/**
 * @brief LoRa transport event handler prototype.
 *
 * @param[in] evt The event that occurred.
 * @param[in] ctx Pointer to the event context.
 */
typedef void (*lora_transport_handler_t)(enum lora_transport_evt evt,
                                         void *ctx);

#include <lora/lora_backend.h>

struct ares_lora;
struct ares_lora_transport;

/**
 * @struct ares_lora_transport_api
 * @brief Unified LoRa transport interface.
 */
struct ares_lora_transport_api {
    /**
     * @brief Function for initializing the LoRa transport interface.
     *
     * @param[in] transport Pointer to the transfer instance.
     * @param[in] config Pointer to the instance configuration.
     * @param[in] evt_handler Event handler.
     * @param[in] context Pointer to the context passed to event handler.
     *
     * @return 0 on success.
     * @return negative error code otherwise.
     */
    int (*init)(const struct ares_lora_transport *transport, const void *config,
                lora_transport_handler_t evt_handler, void *context);

    /**
     * @brief Function for uninitializing the LoRa transport interface.
     *
     * @param[in] transport Pointer to the transfer instance.
     *
     * @return 0 on success.
     * @return negative error code otherwise.
     */
    int (*uninit)(const struct ares_lora_transport *transport);

    /**
     * @brief Function for enabling transport in given TX mode.
     *
     * Function can be used to reconfigure TX to work in blocking mode.
     *
     * @param[in] transport Pointer to transfer instance.
     * @param[in] block_tx If true, the transport TX is enabled in blocking
     * mode.
     *
     * @return 0 on success.
     * @return negative error code otherwise.
     */
    int (*enable)(const struct ares_lora_transport *transport, bool block_tx);

    /**
     * @brief Function for writing data to the transfer interface.
     *
     * @param[in] transport Pointer to the transfer interface.
     * @param[in] data Pointer to the source buffer.
     * @param[in] length Source buffer length.
     * @param[out] cnt Pointer to the sent bytes counter.
     *
     * @return 0 on success.
     * @return negative error code otherwise.
     */
    int (*write)(const struct ares_lora_transport *transport, const void *data,
                 size_t length, size_t *cnt);

    /**
     * @brief Function for reading data from the transport interface.
     *
     * @param[in] transport Pointer to the transfer instance.
     * @param[in] data Pointer to the destination buffer.
     * @param[in] length Destination buffer length.
     * @param[out] cnt Pointer to the received bytes counter.
     *
     * @return 0 on success.
     * @return negative error code otherwise.
     */
    int (*read)(const struct ares_lora_transport *transport, void *data,
                size_t length, size_t *cnt);

    /**
     * @brief Function for reconfiguring the LoRa modem defined in the transport
     * interface.
     *
     * @param[in] transport Pointer to the transfer instance.
     * @param[in] config Pointer to the new LoRa modem configurations.
     *
     * @return 0 on success.
     * @return negative error code otherwise.
     */
    int (*configure)(const struct ares_lora_transport *transport,
                     const struct lora_modem_config *config);
};

struct ares_lora_transport {
    const struct ares_lora_transport_api *api;
    void *ctx;
};

enum ares_lora_signal {
    ARES_LORA_SIGNAL_RXRDY,
    ARES_LORA_SIGNAL_TXDONE,

    ARES_LORA_SIGNALS,
};

#if defined(CONFIG_ARES_LORA_TRX_BUF_SIZE)
#define ARES_LORA_TRX_BUF_SIZE CONFIG_ARES_LORA_TRX_BUF_SIZE
#else
#define ARES_LORA_TRX_BUF_SIZE 512
#endif

struct ares_lora_buf {
    uint8_t buf[ARES_LORA_TRX_BUF_SIZE];
    size_t len;
};

struct ares_lora_command {
    enum ares_packet_payload_type type;
    void (*handler)(const struct ares_lora *lora,
                    const struct ares_packet *packet);
};

struct ares_lora_ctx {
    struct ares_lora_buf rx_buf;
    struct ares_lora_buf tx_buf;

    struct k_poll_signal signals[ARES_LORA_SIGNALS];
    struct k_poll_event events[ARES_LORA_SIGNALS];

    const struct ares_lora_command *commands;
    size_t num_commands;

    struct k_mutex wr_mtx;
    k_tid_t tid;

    uint16_t packet_id;
    uint8_t seq_num;
};

struct ares_lora {
    const struct ares_lora_transport *iface;
    struct ares_lora_ctx *ctx;
    const char *name;
    struct k_thread *thread;
    k_thread_stack_t *stack;
};

#define ARES_LORA_DEFINE(_name, _transport)                                    \
    static struct ares_lora_ctx UTIL_CAT(_name, _ctx);                         \
    static K_KERNEL_STACK_DEFINE(UTIL_CAT(_name, _stack),                      \
                                 CONFIG_ARES_LORA_STACK_SIZE);                 \
    static struct k_thread UTIL_CAT(_name, _thread);                           \
    static const struct ares_lora _name = {.iface = (_transport),              \
                                           .ctx = &UTIL_CAT(_name, _ctx),      \
                                           .name = STRINGIFY(_name),           \
                                           .thread =                           \
                                               &UTIL_CAT(_name, _thread),      \
                                           .stack = UTIL_CAT(_name, _stack)}

int ares_lora_init(const struct ares_lora *lora, const void *transport_config);
int ares_lora_register_command_callbacks(
    const struct ares_lora *lora, const struct ares_lora_command *commands,
    size_t num_commands);
int ares_lora_write_packet(const struct ares_lora *lora,
                           const struct ares_packet *packet);
int ares_lora_configure_lora(const struct ares_lora *lora,
                             const struct lora_modem_config *config);
int ares_lora_set_packet_id(const struct ares_lora *lora,
                            struct ares_packet *packet);

#endif // ARES_LORA_H