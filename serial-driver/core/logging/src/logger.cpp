//
// Created by tschmitz on 11/14/25.
//

#include <../include/logging/logger.hpp>
#include <cstdarg>
#include <cstdio>

#if defined(USE_PYTHON_LOGGERS)
#include <memory>
#include <pybind11/embed.h>
namespace py = pybind11;
#endif

constexpr const char *reset_color = "\033[0m";
constexpr const char *dbg_color = reset_color;
constexpr const char *inf_color = "\033[38;2;39;163;105m";
constexpr const char *wrn_color = "\033[38;2;163;115;76m";
constexpr const char *err_color = "\033[38;2;193;29;40m";
constexpr const char *crit_color = "\033[38;2;117;80;123m";

#if defined(USE_PYTHON_LOGGERS)
class __attribute__((visibility("hidden"))) PieceOfShitIdiom {
  public:
    py::object logger;

    py::object DBG;
    py::object INFO;
    py::object WARNING;
    py::object ERROR;
    py::object CRITICAL;
    py::object OFF;

    explicit PieceOfShitIdiom(const char *name, Logger::LogLevel level) {
        py::module_ mod_ = py::module_::import("logging");
        logger = mod_.attr("getLogger")(name);
        DBG = mod_.attr("DEBUG");
        INFO = mod_.attr("INFO");
        WARNING = mod_.attr("WARNING");
        ERROR = mod_.attr("ERROR");
        CRITICAL = mod_.attr("CRITICAL");
        OFF = CRITICAL + py::int_(10);
        set_level(level);
    }

    void log(Logger::LogLevel level, const char *msg) {
        switch (level) {
        case Logger::LogLevel::LOG_LEVEL_DBG: {
            logger.attr("debug")(msg);
            break;
        }
        case Logger::LogLevel::LOG_LEVEL_INFO: {
            logger.attr("info")(msg);
            break;
        }
        case Logger::LogLevel::LOG_LEVEL_WARN: {
            logger.attr("warning")(msg);
            break;
        }
        case Logger::LogLevel::LOG_LEVEL_ERROR: {
            logger.attr("error")(msg);
            break;
        }
        case Logger::LogLevel::LOG_LEVEL_CRITICAL: {
            logger.attr("critical")(msg);
            break;
        }
        default: {
            break;
        }
        }
    }

    void set_level(Logger::LogLevel level) {
        switch (level) {
        case Logger::LogLevel::LOG_LEVEL_DBG: {
            logger.attr("setLevel")(DBG);
            break;
        }
        case Logger::LogLevel::LOG_LEVEL_INFO: {
            logger.attr("setLevel")(INFO);
            break;
        }
        case Logger::LogLevel::LOG_LEVEL_WARN: {
            logger.attr("setLevel")(WARNING);
            break;
        }
        case Logger::LogLevel::LOG_LEVEL_ERROR: {
            logger.attr("setLevel")(ERROR);
            break;
        }
        case Logger::LogLevel::LOG_LEVEL_CRITICAL: {
            logger.attr("setLevel")(CRITICAL);
            break;
        }
        case Logger::LogLevel::LOG_LEVEL_OFF: {
            logger.attr("setLevel")(OFF);
            break;
        }
        default: {
            break;
        }
        }
    }

    [[nodiscard]] Logger::LogLevel get_level() const {
        int level = logger.attr("level").cast<int>();
        Logger::LogLevel ret;

        // Unfortunately, return values from functions cannot be used in switch
        // statements. To make sure we aren't using magic numbers and staying
        // compatible with future releases we have to do an ugly if-else
        // chain...
        if (level < INFO.cast<int>()) {
            ret = Logger::LogLevel::LOG_LEVEL_DBG;
        } else if (level < WARNING.cast<int>()) {
            ret = Logger::LogLevel::LOG_LEVEL_INFO;
        } else if (level < ERROR.cast<int>()) {
            ret = Logger::LogLevel::LOG_LEVEL_WARN;
        } else if (level < CRITICAL.cast<int>()) {
            ret = Logger::LogLevel::LOG_LEVEL_ERROR;
        } else if (level < OFF.cast<int>()) {
            ret = Logger::LogLevel::LOG_LEVEL_CRITICAL;
        } else {
            ret = Logger::LogLevel::LOG_LEVEL_OFF;
        }

        return ret;
    }
};
#endif

Logger::Logger(const char *name, LogLevel level) {
#if defined(USE_PYTHON_LOGGERS)
    _impl =
        std::unique_ptr<PieceOfShitIdiom>(new PieceOfShitIdiom(name, level));
#else
    _name = name;
    _level = level;
#endif
}

void Logger::set_log_level(LogLevel level) {
#if defined(USE_PYTHON_LOGGERS)
    _impl->set_level(level);
#else
    _level = level;
#endif
}

Logger::LogLevel Logger::get_log_level() const {
#if defined(USE_PYTHON_LOGGERS)
    return _impl->get_level();
#else
    return _level;
#endif
}

void Logger::log(LogLevel level, const char *fmt, ...) const {
    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);

    int len = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);

    if (len < 0) {
        return;
    }

    char *msg = new char[len + 1];
    vsnprintf(msg, len + 1, fmt, args);
    va_end(args);
#if defined(USE_PYTHON_LOGGERS)
    _impl->log(level, msg);
#else
    switch (level) {
    case LOG_LEVEL_DBG: {
        _log_dbg(msg);
        break;
    }
    case LOG_LEVEL_INFO: {
        _log_inf(msg);
        break;
    }
    case LOG_LEVEL_WARN: {
        _log_wrn(msg);
        break;
    }
    case LOG_LEVEL_ERROR: {
        _log_err(msg);
        break;
    }
    case LOG_LEVEL_CRITICAL: {
        _log_crit(msg);
        break;
    }
    default:
        break;
    }
#endif

    delete[] msg;
}

#if !defined(USE_PYTHON_LOGGERS)
void Logger::_log_dbg(const char *msg) const {
    if (_level == LOG_LEVEL_DBG) {
        printf("%s[DBG]%s %s: %s\n", dbg_color, reset_color, _name, msg);
    }
}

void Logger::_log_inf(const char *msg) const {
    if (_level <= LOG_LEVEL_INFO) {
        printf("%s[INFO]%s %s: %s\n", inf_color, reset_color, _name, msg);
    }
}

void Logger::_log_wrn(const char *msg) const {
    if (_level <= LOG_LEVEL_WARN) {
        printf("%s[WARN]%s %s: %s\n", wrn_color, reset_color, _name, msg);
    }
}

void Logger::_log_err(const char *msg) const {
    if (_level <= LOG_LEVEL_ERROR) {
        printf("%s[ERR]%s %s: %s\n", err_color, reset_color, _name, msg);
    }
}

void Logger::_log_crit(const char *msg) const {
    if (_level <= LOG_LEVEL_CRITICAL) {
        printf("%s[CRIT]%s %s: %s\n", crit_color, reset_color, _name, msg);
    }
}
#endif
