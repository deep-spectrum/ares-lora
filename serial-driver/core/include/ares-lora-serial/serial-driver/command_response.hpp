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
#include <vector>

namespace py = pybind11;

struct CommandResponse {
    std::vector<int> error_codes;
    std::vector<std::any> response_values;

    void clear();

    template <typename Expected = void>
    [[nodiscard]] py::tuple build_python_response() const;

  private:
    static py::object specific_ret(const AresFrame::Setting &ret);
    static py::object specific_ret(const AresFrame::Led &ret);
    static py::object specific_ret(const AresFrame::Heartbeat &ret);
    static py::tuple specific_ret(const AresFrame::Version &ret);
    static py::object specific_ret(const AresFrame::BleState &ret);
    // TODO: How to deal with NodeConfigResponse

    template <typename T>
    [[nodiscard]] py::tuple get_additional_response() const;

    template <typename T> static py::object specific_ret(const T &ret) {
        ARG_UNUSED(ret);
        return py::none();
    }
};

template <typename Expected>
py::tuple CommandResponse::build_python_response() const {
    py::tuple codes =
        ares::array_to_tuple(error_codes.data(), error_codes.size());

    if (response_values.empty()) {
        return py::make_tuple(codes, py::none());
    }

    return py::make_tuple(codes,
                          this->template get_additional_response<Expected>());
}

template <typename T>
py::tuple CommandResponse::get_additional_response() const {
    std::vector<py::object> resp;

    for (const auto &i : response_values) {
        resp.emplace_back(specific_ret(std::any_cast<T>(i)));
    }

    return ares::array_to_tuple(resp.data(), resp.size());
}

#endif // ARES_COMMAND_RESPONSE_HPP
