#ifndef _LOG_H_
#define _LOG_H_

#if LOG >= 1
#define warn(...) Serial.printf(__VA_ARGS__)
#else
#define warn(...)
#endif // warn

#if LOG >= 2
#define info(...) Serial.printf(__VA_ARGS__)
#else
#define info(...)
#endif // info

#if LOG >= 3
#define trace(...) Serial.printf(__VA_ARGS__)
#else
#define trace(...)
#endif // trace

#endif // !_LOG_H_
