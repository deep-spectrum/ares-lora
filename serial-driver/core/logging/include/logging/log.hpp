/**
 * @file log.hpp
 *
 * @brief Logger helper macros.
 *
 * @date 11/17/2025
 *
 * @author Tom Schmitz \<tschmitz@andrew.cmu.edu\>
 */

#ifndef LOGGING_LOG_HPP
#define LOGGING_LOG_HPP

#include <logging/internal/logging_utils.h>
#include <logging/logger.hpp>

/**
 * Logging level values. These should not be used to set logging levels.
 * @{
 */
#define Z_LOG_LEVEL_DBG  0
#define Z_LOG_LEVEL_INF  1
#define Z_LOG_LEVEL_WRN  2
#define Z_LOG_LEVEL_ERR  3
#define Z_LOG_LEVEL_CRIT 4
#define Z_LOG_LEVEL_OFF  5
/**
 * @}
 */

/**
 * Default logging level.
 * @{
 */
#if LOG_LEVEL == Z_LOG_LEVEL_DBG
#define DEFAULT_LOG_LEVEL Logger::LogLevel::LOG_LEVEL_DBG
#elif LOG_LEVEL == Z_LOG_LEVEL_INF
#define DEFAULT_LOG_LEVEL Logger::LogLevel::LOG_LEVEL_INFO
#elif LOG_LEVEL == Z_LOG_LEVEL_WRN
#define DEFAULT_LOG_LEVEL Logger::LogLevel::LOG_LEVEL_WARN
#elif LOG_LEVEL == Z_LOG_LEVEL_ERR
#define DEFAULT_LOG_LEVEL Logger::LogLevel::LOG_LEVEL_ERROR
#elif LOG_LEVEL == Z_LOG_LEVEL_CRIT
#define DEFAULT_LOG_LEVEL Logger::LogLevel::LOG_LEVEL_CRITICAL
#else
#define DEFAULT_LOG_LEVEL Logger::LogLevel::LOG_LEVEL_OFF
#endif
/**
 * @}
 */

#define Z_REGISTER_LOGGER_DEFAULT(name_)                                       \
    static const char *__name__ = #name_;                                      \
    static Logger __logger__(__name__, DEFAULT_LOG_LEVEL);                     \
    static Logger::LogLevel __saved_level__ = DEFAULT_LOG_LEVEL;
#define Z_REGISTER_LOGGER(name_, level_)                                       \
    static const char *__name__ = #name_;                                      \
    static Logger __logger__(__name__, Logger::LogLevel::level_);              \
    static Logger::LogLevel __saved_level__ = Logger::LogLevel::level_;

/**
 * Registers a module specific logger. This takes in the module name and
 * optionally, the logging level of the module.
 * @param name_ The module name.
 * @param level_ (Optional) The default logging level of the module.
 */
#define LOG_MODULE_REGISTER(name_, level_...)                                  \
    COND_CODE_0(IS_EMPTY(level_), (Z_REGISTER_LOGGER(name_, level_)),          \
                (Z_REGISTER_LOGGER_DEFAULT(name_)))

/**
 * The module name registered with @ref LOG_MODULE_REGISTER
 */
#define LOG_MODULE_NAME __name__

/**
 * @brief Writes a DBG level message to the log.
 *
 * @param[in] msg_ A string optionally containing printf valid conversion
 * specifier, followed by as many values as specifiers.
 */
#define LOG_DBG(msg_, ...)                                                     \
    COND_CODE_0(                                                               \
        IS_EMPTY(__VA_ARGS__),                                                 \
        (__logger__.log(Logger::LogLevel::LOG_LEVEL_DBG, msg_, __VA_ARGS__)),  \
        (__logger__.log(Logger::LogLevel::LOG_LEVEL_DBG, msg_)))

/**
 * @brief Writes an INFO level message to the log.
 *
 * @param[in] msg_ A string optionally containing printf valid conversion
 * specifier, followed by as many values as specifiers.
 */
#define LOG_INF(msg_, ...)                                                     \
    COND_CODE_0(                                                               \
        IS_EMPTY(__VA_ARGS__),                                                 \
        (__logger__.log(Logger::LogLevel::LOG_LEVEL_INFO, msg_, __VA_ARGS__)), \
        (__logger__.log(Logger::LogLevel::LOG_LEVEL_INFO, msg_)))

/**
 * @brief Writes a WARNING level message to the log.
 *
 * @param[in] msg_ A string optionally containing printf valid conversion
 * specifier, followed by as many values as specifiers.
 */
#define LOG_WRN(msg_, ...)                                                     \
    COND_CODE_0(                                                               \
        IS_EMPTY(__VA_ARGS__),                                                 \
        (__logger__.log(Logger::LogLevel::LOG_LEVEL_WARN, msg_, __VA_ARGS__)), \
        (__logger__.log(Logger::LogLevel::LOG_LEVEL_WARN, msg_)))

/**
 * @brief Writes an ERROR level message to the log.
 *
 * @param[in] msg_ A string optionally containing printf valid conversion
 * specifier, followed by as many values as specifiers.
 */
#define LOG_ERR(msg_, ...)                                                     \
    COND_CODE_0(IS_EMPTY(__VA_ARGS__),                                         \
                (__logger__.log(Logger::LogLevel::LOG_LEVEL_ERROR, msg_,       \
                                __VA_ARGS__)),                                 \
                (__logger__.log(Logger::LogLevel::LOG_LEVEL_ERROR, msg_)))

/**
 * @brief Writes a CRITICAL ERROR level message to the log.
 *
 * @param[in] msg_ A string optionally containing printf valid conversion
 * specifier, followed by as many values as specifiers.
 */
#define LOG_CRIT(msg_, ...)                                                    \
    COND_CODE_0(IS_EMPTY(__VA_ARGS__),                                         \
                (__logger__.log(Logger::LogLevel::LOG_LEVEL_CRITICAL, msg_,    \
                                __VA_ARGS__)),                                 \
                (__logger__.log(Logger::LogLevel::LOG_LEVEL_CRITICAL, msg_)))

#define LOG_DBG_HEXDUMP(bytes_, len_, msg_)                                    \
    __logger__.log_hexdump(Logger::LogLevel::LOG_LEVEL_DBG, msg_, bytes_, len_)
#define LOG_INF_HEXDUMP(bytes_, len_, msg_)                                    \
    __logger__.log_hexdump(Logger::LogLevel::LOG_LEVEL_INFO, msg_, bytes_, len_)
#define LOG_WRN_HEXDUMP(bytes_, len_, msg_)                                    \
    __logger__.log_hexdump(Logger::LogLevel::LOG_LEVEL_WARN, msg_, bytes_, len_)
#define LOG_ERR_HEXDUMP(bytes_, len_, msg_)                                    \
    __logger__.log_hexdump(Logger::LogLevel::LOG_LEVEL_ERROR, msg_, bytes_,    \
                           len_)
#define LOG_CRIT_HEXDUMP(bytes_, len_, msg_)                                   \
    __logger__.log_hexdump(Logger::LogLevel::LOG_LEVEL_CRITICAL, msg_, bytes_, \
                           len_)

/**
 * Saves the old logging level and sets the new logging level.
 * @param[in] new_level The new logging level of the module.
 *
 * @note This will only override the logging level if the old level is more
 * restrictive. If this is not desired behavior, use
 * @ref SAVE_LOG_LEVEL_AND_FORCE().
 */
#define SAVE_LOG_LEVEL_AND_OVERRIDE(new_level)                                 \
    do {                                                                       \
        __saved_level__ = __logger__.get_log_level();                          \
        if (Logger::LogLevel::new_level < __saved_level__) {                   \
            __logger__.set_log_level(Logger::LogLevel::new_level);             \
        }                                                                      \
    } while (false)

/**
 * Saves the old logging level and forces the new logging level.
 * @param new_level The new logging level of the module.
 *
 * @note This will override the logging level no matter what. If you want to
 * avoid overriding the logging level if the current level is more permissive,
 * the see @ref SAVE_LOG_LEVEL_AND_OVERRIDE().
 */
#define SAVE_LOG_LEVEL_AND_FORCE(new_level)                                    \
    do {                                                                       \
        __saved_level__ = __logger__.get_log_level();                          \
        __logger__.set_log_level(Logger::LogLevel::new_level);                 \
    } while (false)

/**
 * Restores the logging level back to its saved old level.
 *
 * @see SAVE_LOG_LEVEL_AND_OVERRIDE
 * @see SAVE_LOG_LEVEL_AND_FORCE
 */
#define RESTORE_LOG_LEVEL()                                                    \
    do {                                                                       \
        __logger__.set_log_level(__saved_level__);                             \
    } while (false)

#endif // LOGGING_LOG_HPP