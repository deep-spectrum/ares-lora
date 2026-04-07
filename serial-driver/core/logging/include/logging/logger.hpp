/**
 * @file logger.hpp
 *
 * @brief C++ logger implementation.
 *
 * @date 11/14/2025
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef VERSION_LOGGER_HPP
#define VERSION_LOGGER_HPP

#include <cstdint>
#include <vector>

#if defined(USE_PYTHON_LOGGERS)
#include <memory>
class PieceOfShitIdiom;
#endif // defined(USE_PYTHON_LOGGERS)

/**
 * @class Logger
 * Implementation of C++ logger.
 *
 * @note If using a Python logger, it should be noted that `OFF` is considered
 * to be `logging.CRITICAL + 10`.
 */
class Logger {
  public:
    /**
     * Logging levels.
     */
    enum LogLevel : unsigned int {
        LOG_LEVEL_DBG = 0,      ///< Debug logging level.
        LOG_LEVEL_INFO = 1,     ///< Info logging level.
        LOG_LEVEL_WARN = 2,     ///< Warning logging level.
        LOG_LEVEL_ERROR = 3,    ///< Error logging level.
        LOG_LEVEL_CRITICAL = 4, ///< Critical logging level.
        LOG_LEVEL_OFF = 5,      ///< Logging turned off.
    };

    /**
     * .
     * @param[in] name The name of the logger.
     * @param[in] level The starting logging level.
     */
    explicit Logger(const char *name, LogLevel level);

    /**
     * .
     */
    ~Logger() = default;

    /**
     * .
     * @param[in] level The new logging level.
     */
    void set_log_level(LogLevel level);

    /**
     * .
     * @return The current logging level.
     */
    [[nodiscard]] LogLevel get_log_level() const;

    /**
     * @brief Log a message with the given level.
     *
     * Logs the given message with the given  level. If the loggers level is set
     * higher than the given level, then the message will not be logged.
     *
     * @param[in] level The logging message type. Will do nothing if set to
     * `LOG_LEVEL_OFF`.
     * @param[in] fmt The format string for the log message.
     * @param[in] ... Additional parameters needed for the format string.
     */
    void log(LogLevel level, const char *fmt, ...) const;

    void log_hexdump(LogLevel level, const char *msg,
                     const std::vector<uint8_t> &buf, std::size_t bytes);

  private:
#if defined(USE_PYTHON_LOGGERS)
    std::unique_ptr<PieceOfShitIdiom> _impl;
#else
    const char *_name;
    LogLevel _level;

    void _log_dbg(const char *msg) const;
    void _log_inf(const char *msg) const;
    void _log_wrn(const char *msg) const;
    void _log_err(const char *msg) const;
    void _log_crit(const char *msg) const;
#endif // defined(USE_PYTHON_LOGGERS)
};

#endif // VERSION_LOGGER_HPP