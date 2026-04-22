/**
 * @file lora.h
 *
 * @brief LoRa API.
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

/**
 * @struct ares_lora_transport
 * @brief Transfer interface.
 */
struct ares_lora_transport {
    /**
     * Transfer interface API.
     */
    const struct ares_lora_transport_api *api;

    /**
     * Transfer interface context.
     */
    void *ctx;
};

/**
 * @enum ares_lora_signal
 * @brief Signals for the LoRa interface.
 */
enum ares_lora_signal {
    ARES_LORA_SIGNAL_RXRDY,  ///< Rx data ready.
    ARES_LORA_SIGNAL_TXDONE, ///< Done transmitting. Must be last one before
                             ///< ARES_LORA_SIGNALS

    ARES_LORA_SIGNALS, ///< Last enumerator.
};

#if defined(CONFIG_ARES_LORA_TRX_BUF_SIZE)
/**
 * Unified LoRa buffer size for transmitting and receiving.
 */
#define ARES_LORA_TRX_BUF_SIZE CONFIG_ARES_LORA_TRX_BUF_SIZE
#else
/**
 * Unified LoRa buffer size for transmitting and receiving.
 */
#define ARES_LORA_TRX_BUF_SIZE 512
#endif // defined(CONFIG_ARES_LORA_TRX_BUF_SIZE)

/**
 * @struct ares_lora_buf
 * @brief Unified buffer structure for LoRa.
 */
struct ares_lora_buf {
    /**
     * Buffer for storing bytes.
     */
    uint8_t buf[ARES_LORA_TRX_BUF_SIZE];

    /**
     * Number of valid bytes starting from 0.
     */
    size_t len;
};

/**
 * @struct ares_lora_command
 * @brief LoRa packet payload handler descriptor.
 */
struct ares_lora_command {
    /**
     * The payload type to handle.
     */
    enum ares_packet_payload_type type;

    /**
     * The Packet handler.
     *
     * @param[in] lora Pointer to LoRa instance.
     * @param[in] packet The received packet to handle.
     */
    void (*handler)(const struct ares_lora *lora,
                    const struct ares_packet *packet);
};

/**
 * @struct ares_lora_ctx
 * @brief Ares LoRa instance context.
 */
struct ares_lora_ctx {
    /**
     * LoRa receive buffer.
     */
    struct ares_lora_buf rx_buf;

    /**
     * Lora transmit buffer.
     */
    struct ares_lora_buf tx_buf;

    /**
     * LoRa event signals.
     */
    struct k_poll_signal signals[ARES_LORA_SIGNALS];

    /**
     * LoRa events.
     */
    struct k_poll_event events[ARES_LORA_SIGNALS];

    /**
     * Array of LoRa handlers for received packets.
     */
    const struct ares_lora_command *commands;

    /**
     * The number of LoRa handlers.
     */
    size_t num_commands;

    /**
     * Write mutex.
     */
    struct k_mutex wr_mtx;

    /**
     * Thread ID for LoRa.
     */
    k_tid_t tid;

    /**
     * The next packet ID.
     */
    uint16_t packet_id;

    /**
     * The next sequence number.
     */
    uint8_t seq_num;
};

/**
 * @struct ares_lora
 * @brief Ares LoRa internals.
 */
struct ares_lora {
    /**
     * Transport interface.
     */
    const struct ares_lora_transport *iface;

    /**
     * Internal context.
     */
    struct ares_lora_ctx *ctx;

    /**
     * Instance name.
     */
    const char *name;

    /**
     * Instance thread.
     */
    struct k_thread *thread;

    /**
     * Thread stack.
     */
    k_thread_stack_t *stack;
};

/**
 * @brief Macro for defining an ares_lora instance.
 *
 * @param[in] _name Instance name.
 * @param[in] _transport Pointer to the transport interface.
 */
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

/**
 * @brief Function for initializing a transport layer and internal lora state.
 *
 * @param[in] lora Pointer to lora instance.
 * @param[in] transport_config Transport configuration during initialization.
 *
 * @return 0 on success.
 * @return negative error code otherwise.
 */
int ares_lora_init(const struct ares_lora *lora, const void *transport_config);

/**
 * @brief Function for registering the lora packet handlers.
 *
 * @param[in] lora Pointer to lora instance.
 * @param[in] commands The array of lora packet handlers.
 * @param[in] num_commands The number of entries in the array.
 *
 * @return 0 on success.
 * @return -EINVAL if parameters are invalid.
 */
int ares_lora_register_command_callbacks(
    const struct ares_lora *lora, const struct ares_lora_command *commands,
    size_t num_commands);

/**
 * @brief Function to write a packet to the LoRa transfer layer.
 *
 * @param[in] lora Pointer to lora instance.
 * @param[in] packet Pointer to packet to send over LoRa.
 *
 * @return 0 on success.
 * @return -EINVAL if parameters are invalid.
 * @return negative error code otherwise.
 *
 * @note This automatically manages the sequence number of the packet.
 */
int ares_lora_write_packet(const struct ares_lora *lora,
                           const struct ares_packet *packet);

/**
 * @brief Function to send new configurations to the LoRa transfer layer.
 *
 * @param[in] lora Pointer to lora instance.
 * @param[in] config Pointer to new LoRa modem configurations.
 *
 * @return 0 on success.
 * @return -EINVAL if parameters are invalid.
 * @return negative error code otherwise.
 */
int ares_lora_configure_lora(const struct ares_lora *lora,
                             const struct lora_modem_config *config);

/**
 * @brief Function to update the packet ID of the given packet from the lora
 * instance.
 *
 * @param[in] lora Pointer to lora instance.
 * @param[in,out] packet Pointer to packet to update the packet ID for.
 *
 * @return 0 on success.
 * @return -EINVAL if parameters are invalid.
 */
int ares_lora_set_packet_id(const struct ares_lora *lora,
                            struct ares_packet *packet);

#endif // ARES_LORA_H