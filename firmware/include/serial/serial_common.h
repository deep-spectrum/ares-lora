/**
 * @file serial_common.h
 *
 * @brief
 *
 * @date 4/15/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_SERIAL_COMMON_H
#define ARES_SERIAL_COMMON_H

#if defined(CONFIG_ARES_SERIAL_TX_RINGBUF_LEN)
#define SERIAL_BACKEND_TX_RINGBUF_SIZE CONFIG_ARES_SERIAL_TX_RINGBUF_LEN
#else
#define SERIAL_BACKEND_TX_RINGBUF_SIZE 256
#endif

#if defined(CONFIG_ARES_SERIAL_RX_RINGBUF_LEN)
#define SERIAL_BACKEND_RX_RINGBUF_SIZE CONFIG_ARES_SERIAL_RX_RINGBUF_LEN
#else
#define SERIAL_BACKEND_RX_RINGBUF_SIZE 256
#endif

#endif // ARES_SERIAL_COMMON_H
