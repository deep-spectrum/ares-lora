/**
 * @file ble_types.hpp
 *
 * @brief
 *
 * @date 11/6/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_BLE_TYPES_HPP
#define ARES_BLE_TYPES_HPP

#include <cstdint>

namespace AresFrame {
/**
 * @enum BlePhy
 * BLE Physical layer options.
 */
enum BlePhy : uint8_t {
    PHY_2M = 0,       ///< 2Mbit/sec
    PHY_1M = 1,       ///< 1Mbit/sec
    PHY_S2_CODED = 2, ///< Coded S=2
    PHY_S8_CODED = 3, ///< Coded S=8
};

/**
 * @enum BtAttError
 * Bluetooth ATT errors.
 */
enum BtAttError : uint8_t {
    /* Error codes for Error response PDU
     *
     * Defined by The Bluetooth Core Specification, Version 5.4, Vol 3, Part F,
     * Section 3.4.1.1
     */
    /** The ATT operation was successful */
    BT_ATT_ERR_SUCCESS = 0x00,
    /** The attribute handle given was not valid on the server */
    BT_ATT_ERR_INVALID_HANDLE = 0x01,
    /** The attribute cannot be read */
    BT_ATT_ERR_READ_NOT_PERMITTED = 0x02,
    /** The attribute cannot be written */
    BT_ATT_ERR_WRITE_NOT_PERMITTED = 0x03,
    /** The attribute PDU was invalid */
    BT_ATT_ERR_INVALID_PDU = 0x04,
    /** The attribute requires authentication before it can be read or written
     */
    BT_ATT_ERR_AUTHENTICATION = 0x05,
    /** The ATT Server does not support the request received from the client */
    BT_ATT_ERR_NOT_SUPPORTED = 0x06,
    /** Offset specified was past the end of the attribute */
    BT_ATT_ERR_INVALID_OFFSET = 0x07,
    /** The attribute requires authorization before it can be read or written */
    BT_ATT_ERR_AUTHORIZATION = 0x08,
    /** Too many prepare writes have been queued */
    BT_ATT_ERR_PREPARE_QUEUE_FULL = 0x09,
    /** No attribute found within the given attribute handle range */
    BT_ATT_ERR_ATTRIBUTE_NOT_FOUND = 0x0a,
    /** The attribute cannot be read using the ATT_READ_BLOB_REQ PDU */
    BT_ATT_ERR_ATTRIBUTE_NOT_LONG = 0x0b,
    /** The Encryption Key Size used for encrypting this link is too short */
    BT_ATT_ERR_ENCRYPTION_KEY_SIZE = 0x0c,
    /** The attribute value length is invalid for the operation */
    BT_ATT_ERR_INVALID_ATTRIBUTE_LEN = 0x0d,
    /**
     * @brief The attribute request that was requested has encountered an error
     * that was unlikely
     *
     * The attribute request could therefore not be completed as requested
     */
    BT_ATT_ERR_UNLIKELY = 0x0e,
    /** The attribute requires encryption before it can be read or written */
    BT_ATT_ERR_INSUFFICIENT_ENCRYPTION = 0x0f,
    /**
     * @brief The attribute type is not a supported grouping attribute
     *
     * The attribute type is not a supported grouping attribute as defined by a
     * higher layer specification.
     */
    BT_ATT_ERR_UNSUPPORTED_GROUP_TYPE = 0x10,
    /** Insufficient Resources to complete the request */
    BT_ATT_ERR_INSUFFICIENT_RESOURCES = 0x11,
    /** The server requests the client to rediscover the database */
    BT_ATT_ERR_DB_OUT_OF_SYNC = 0x12,
    /** The attribute parameter value was not allowed */
    BT_ATT_ERR_VALUE_NOT_ALLOWED = 0x13,

    /* Common Profile Error Codes
     *
     * Defined by the Supplement to the Bluetooth Core Specification (CSS), v11,
     * Part B, Section 1.2.
     */
    /** Write Request Rejected */
    BT_ATT_ERR_WRITE_REQ_REJECTED = 0xfc,
    /** Client Characteristic Configuration Descriptor Improperly Configured */
    BT_ATT_ERR_CCC_IMPROPER_CONF = 0xfd,
    /** Procedure Already in Progress */
    BT_ATT_ERR_PROCEDURE_IN_PROGRESS = 0xfe,
    /** Out of Range */
    BT_ATT_ERR_OUT_OF_RANGE = 0xff,
};

} // namespace AresFrame

#endif // ARES_BLE_TYPES_HPP
