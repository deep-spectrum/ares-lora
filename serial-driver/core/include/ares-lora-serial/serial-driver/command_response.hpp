/**
 * @file command_response.hpp
 *
 * @brief
 *
 * @date 8/17/26
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef ARES_COMMAND_RESPONSE_HPP
#define ARES_COMMAND_RESPONSE_HPP

#include <any>
#include <ares-lora-serial/frames/frame.hpp>
#include <ares/pyutil.hpp>
#include <pybind11/pybind11.h>
#include <string>
#include <vector>

namespace py = pybind11;

/**
 * @struct CommandResponse
 * Holds the command acknoledgements and any other responses.
 */
struct CommandResponse {
    /**
     * Error codes returned.
     */
    std::vector<int> error_codes;

    /**
     * Command specific responses or responses from LoRa
     */
    std::vector<std::any> response_values;

    /**
     * Clear the responses.
     */
    void clear();

    /**
     * Convert the command response into something Python can understand.
     * @tparam Expected The payload class of the expected response.
     * @tparam BadCastException The exception to to throw on bad cast.
     * @param[in] exc_msg The exception message.
     * @return tuple[tuple[int, ...], tuple[responseValue | None\ | None]]
     */
    template <typename Expected = void, typename BadCastException = void>
    [[nodiscard]] py::tuple
    build_python_response(const std::string &exc_msg = "") const;

  private:
    static py::object specific_ret(const AresFrame::Setting &ret);
    static py::object specific_ret(const AresFrame::Led &ret);
    static py::object specific_ret(const AresFrame::Heartbeat &ret);
    static py::tuple specific_ret(const AresFrame::Version &ret);
    static py::object specific_ret(const AresFrame::BleState &ret);
    static py::object specific_ret(const AresFrame::NodeConfigResponse &ret);

    template <typename T, typename BadCastException>
    [[nodiscard]] py::tuple
    get_additional_response(const std::string &exc_msg) const;

    template <typename T> static py::object specific_ret(const T &ret) {
        ARG_UNUSED(ret);
        return py::none();
    }
};

template <typename Expected, typename BadCastException>
py::tuple
CommandResponse::build_python_response(const std::string &exc_msg) const {
    py::tuple codes =
        ares::array_to_tuple(error_codes.data(), error_codes.size());

    if constexpr (std::is_void_v<Expected>) {
        return py::make_tuple(codes, py::none());
    } else {
        if (response_values.empty()) {
            return py::make_tuple(codes, py::none());
        }

        return py::make_tuple(
            codes,
            get_additional_response<Expected, BadCastException>(exc_msg));
    }
}

template <typename T, typename BadCastException>
py::tuple
CommandResponse::get_additional_response(const std::string &exc_msg) const {
    std::vector<py::object> resp;

    for (const auto &i : response_values) {
        if constexpr (std::is_same_v<T, AresFrame::LoraAck> ||
                      std::is_same_v<T, AresFrame::LogAck>) {
            bool value = i.type() == typeid(T);
            resp.emplace_back(py::bool_(value));
        } else if constexpr (!std::is_void_v<BadCastException>) {
            if (i.type() == typeid(std::monostate)) {
                throw BadCastException(exc_msg);
            }
            resp.emplace_back(specific_ret(std::any_cast<T>(i)));
        } else {
            if (i.type() == typeid(T)) {
                resp.emplace_back(specific_ret(std::any_cast<T>(i)));
            } else {
                resp.emplace_back(py::none());
            }
        }
    }

    return ares::array_to_tuple(resp.data(), resp.size());
}

#endif // ARES_COMMAND_RESPONSE_HPP
