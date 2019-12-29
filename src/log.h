#ifndef LOG_H
#define LOG_H

#include <iostream>

extern bool log_verbose;

#ifdef _ANDROID_LOG_
    #include <android/log.h>
    #include <sstream>

    #define LOG_TO(level,message) { \
        std::stringstream sLog; \
        sLog << (level) << " " << __FILE__ << ":" << __LINE__ << " - " << message << std::endl; \
        __android_log_write(level, "Caprice32", sLog.str().c_str()); \
    }

    #define LOG_ERROR(message) LOG_TO(ANDROID_LOG_ERROR, message)
    #define LOG_WARNING(message) LOG_TO(ANDROID_LOG_WARNING, message)
    #define LOG_INFO(message) LOG_TO(ANDROID_LOG_INFO, message)
    #define LOG_VERBOSE(message) if(log_verbose) { LOG_TO(ANDROID_LOG_VERBOSE, message) }

    #ifdef DEBUG
    #define LOG_DEBUG(message) if(log_verbose) { LOG_TO(ANDROID_LOG_DEBUG, message) }
    #else
    #define LOG_DEBUG(message)
    #endif
#else
    #define LOG_TO(stream,level,message) stream << (level) << " " << __FILE__ << ":" << __LINE__ << " - " << message << std::endl; // NOLINT(misc-macro-parentheses): Not having parentheses around message is a feature, it allows using streams in LOG macros

    #define LOG_ERROR(message) LOG_TO(std::cerr, "ERROR  ", message)
    #define LOG_WARNING(message) LOG_TO(std::cerr, "WARNING", message)
    #define LOG_INFO(message) LOG_TO(std::cerr, "INFO   ", message)
    #define LOG_VERBOSE(message) if(log_verbose) { LOG_TO(std::cout, "VERBOSE", message) }

    #ifdef DEBUG
    #define LOG_DEBUG(message) if(log_verbose) { LOG_TO(std::cout, "DEBUG  ", message) }
    #else
    #define LOG_DEBUG(message)
    #endif
#endif
#endif
