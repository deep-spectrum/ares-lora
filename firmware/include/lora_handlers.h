/**
 * @file lora_handlers.h
 *
 * @brief LoRa packet handler APIs.
 *
 * @date 3/30/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_LORA_HANDLERS_H
#define ARES_LORA_HANDLERS_H

/**
 * Tell the LoRa module that the node ID and/or PAN ID were updated and to sync
 * to the new settings.
 */
void refresh_modem_id(void);

#endif // ARES_LORA_HANDLERS_H
