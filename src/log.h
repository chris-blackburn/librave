/**
 * Logging
 *
 * Basic logging
 *
 * Author: Christopher Blackburn <krizboy@vt.edu>
 * Date: 1/7/2021
 */

#ifndef __LOG_H_
#define __LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define LOG(lvl, fmt, ...) \
    fprintf(stderr, "[rave] %s (%s:%d) " fmt "\n", lvl, __func__, __LINE__, ##__VA_ARGS__);

#ifndef NDEBUG
#define DEBUG(msg, ...) LOG("DEBUG", msg, ##__VA_ARGS__);
#else
#define DEBUG(msg, ...)
#endif /* NDEBUG */
#define INFO(msg, ...)  LOG("INFO ", msg, ##__VA_ARGS__);
#define WARN(msg, ...)  LOG("WARN ", msg, ##__VA_ARGS__);
#define ERROR(msg, ...) LOG("ERROR", msg, ##__VA_ARGS__);
#define FATAL(msg, ...) LOG("FATAL", msg, ##__VA_ARGS__);

#ifdef __cplusplus
}
#endif

#endif /* __LOG_H_ */
