#include "Logging.h"
#include "CurrentThread.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <chrono>
#include <sstream>

namespace eff
{

    __thread char t_errnobuf[512];
    __thread char t_time[64];
    __thread time_t t_lastSecond;

    const char *strerror_tl(int savedErrno)
    {
        //根据错误码得到对应的错误描述。
        return strerror_r(savedErrno, t_errnobuf, sizeof t_errnobuf);
    }

    Logger::LogLevel initLogLevel()
    {
        return Logger::DEBUG;
        // if(::getenv("MUDUO_LOG_TRACE"))
        //     return Logger::TRACE;
        // else if(::getenv("MUDUO_LOG_DEBUG"))
        //     return Logger::DEBUG;
        // else
        //     return Logger::INFO;
    }

    Logger::LogLevel g_logLevel = initLogLevel();

    const char *LogLevelName[Logger::NUM_LOG_LEVELS] =
        {
            "TRACE ",
            "DEBUG ",
            "INFO  ",
            "WARN  ",
            "ERROR ",
            "FATAL ",
    };

    class T
    {
    public:
        T(const char *str, unsigned len)
            : str_(str),
              len_(len)
        {
            assert(strlen(str) == len_);
        }

        const char *str_;
        const unsigned len_;
    };

    inline LogStream &operator<<(LogStream &s, T v)
    {
        s.append(v.str_, v.len_);
        return s;
    }

    inline LogStream &operator<<(LogStream &s, const Logger::SourceFile &v)
    {
        s.append(v.data_, v.size_);
        return s;
    }

    void defaultOutput(const char *msg, int len)
    {
        size_t n = fwrite(msg, 1, len, stdout);

        (void)n;
    }

    void defaultFlush()
    {
        fflush(stdout);
    }

    Logger::OutputFunc g_output = defaultOutput;
    Logger::FlushFunc g_flush = defaultFlush;
}
// TimeZone g_logTimeZone;

using namespace eff;

Logger::Impl::Impl(LogLevel level, int savedErrno, const SourceFile &file, int line)
    : time_(Timestamp::now()), //获取当前时间
      stream_(),
      level_(level),
      line_(line),
      basename_(file)
{
    formatTime();
    // CurrentTread::tid();
    // stream_ << T(CurrentThread::tidString(), CurrentTread::tidStringLength());
    stream_ << T(LogLevelName[level], 6);

    if (savedErrno != 0)
    {
        stream_ << strerror_tl(savedErrno) << " (errno=" << savedErrno << ")";
    }
}

//待完成
// void Logger::Impl::formatTime()
// {
//     int64_t microSecondsSinceEpoch = time_.microSecondsSinceEpoch();
//     time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / Timestamp::kMicroSecondsPerSecond);
//     int microseconds = static_cast<int>(microSecondsSinceEpoch % Timestamp::kMicroSecondsPerSecond);
//     if (seconds != t_lastSecond)
//     {
//         t_lastSecond = seconds;
//         struct tm tm_time;
//         // if(g_logTimeZone.valid())
//         // {
//         //     tm_time = g_logTimeZone.toLocalTime(seconds);
//         // }else{
//         ::gmtime_r(&seconds, &tm_time);
//         //}

//         int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
//                            tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
//                            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
//         assert(len == 17);

//         (void)len;
//     }

//     // if(g_logTimeZone.valid())
//     // {
//     //     Fmt us(".%06d", microseconds);
//     //     assert(us.length() == 8);
//     //     stream_ << T(t_time, 17) << T(us.data(), 8);
//     // }else{
//     Fmt us(".%06dZ ", microseconds);
//     assert(us.length() == 9);
//     stream_ << T(t_time, 17) << T(us.data(), 9);
//     //}
// }

void Logger::Impl::formatTime()
{
    int64_t milli = time_.microSecondsSinceEpoch() / 1000 + (int64_t)8 * 60 * 60 * 1000;
    auto mTime = std::chrono::milliseconds(milli);
    auto tp = std::chrono::time_point<std::chrono::system_clock,
                                      std::chrono::milliseconds>(mTime);
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm *now = std::gmtime(&tt);
    stream_ << now->tm_year + 1900 << '-' << now->tm_mon + 1 << '-'
            << now->tm_mday << ' ' << now->tm_hour << ':' << now->tm_min << ':'
            << now->tm_sec << ' ';
}

void Logger::Impl::finish()
{
    stream_ << " - " << basename_ << ';' << line_ << '\n';
}

Logger::Logger(SourceFile file, int line)
    : impl_(INFO, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level, const char *func)
    : impl_(level, 0, file, line)
{
    impl_.stream_ << func << ' ';
}

Logger::Logger(SourceFile file, int line, LogLevel level)
    : impl_(level, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, bool toAbort)
    : impl_(toAbort ? FATAL : ERROR, errno, file, line)
{
}

Logger::~Logger()
{
    impl_.finish();
    const LogStream::Buffer &buf(stream().buffer());
    g_output(buf.data(), buf.length());
    if (impl_.level_ == FATAL)
    {
        g_flush();
        abort();
    }
}

void Logger::setLogLevel(Logger::LogLevel level)
{
    g_logLevel = level;
}

void Logger::setOutput(OutputFunc out)
{
    g_output = out;
}

void Logger::setFlush(FlushFunc flush)
{
    g_flush = flush;
}

// void Logger::setTimeZone(const TimeZone& tz)
// {
//     g_logTimeZone = tz;
// }
