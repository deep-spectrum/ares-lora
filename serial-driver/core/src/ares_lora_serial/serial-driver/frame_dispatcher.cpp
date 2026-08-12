/**
 * @file frame_dispatcher.cpp
 *
 * @brief
 *
 * @date 8/12/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#include <ares-lora-serial/serial-driver/frame_dispatcher.hpp>
#include <type_traits>

template <typename T, typename = void>
struct has_member_id : std::false_type {};

template <typename T>
struct has_member_id<T, std::void_t<decltype(std::declval<T>().id)>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_member_id_v = has_member_id<T>::value;

template <typename T, typename = void>
struct has_member_broadcast : std::false_type {};

template <typename T>
struct has_member_broadcast<T,
                            std::void_t<decltype(std::declval<T>().broadcast)>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_member_broadcast_v = has_member_broadcast<T>::value;

static void set_lora_destination_id(AresFrame::Frame::TxTypes &payload,
                                    uint16_t destination) {
    std::visit(
        [&destination](auto &obj) {
            if constexpr (has_member_id_v<std::decay_t<decltype(obj)>>) {
                obj.id = destination;
            }
        },
        payload);
}

static void set_lora_broadcast(AresFrame::Frame::TxTypes &payload,
                               bool broadcast) {
    std::visit(
        [&broadcast](auto &obj) {
            if constexpr (has_member_broadcast_v<std::decay_t<decltype(obj)>>) {
                obj.broadcast = broadcast;
            }
        },
        payload);
}

py::tuple FrameDispatcher::send_frame(AresFrame::Frame::TxTypes &payload) {
    set_lora_destination_id(payload, destination);
    set_lora_broadcast(payload, broadcast);

    AresFrame::Frame frame(payload);
    std::vector<AresSerial::FrameResponse> responses;

    _send_frame(frame, response_timeout, responses);

    // todo
    return py::tuple();
}
