#include <chrono>
#include <future>
#include "AsyncLogging.h"
#include "LogFile.h"
#include "Timestamp.h"

using namespace eff;

AsyncLogging::AsyncLogging(const std::string& basename,
                            off_t rollsize,
                            size_t flushInterval)
    :   basename(basename),
        rollSize(rollsize),
        flushInterval_(flushInterval),
        running_(false),
        currentBuffer_(new Buffer),
        nextBuffer_(new Buffer),
        buffers_(),
        thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging")
{
    currentBuffer_->bzero();
    nextBuffer_->bzero(); //memset
    buffers_.reserve(16); //提前分配空间
}



void AsyncLogging::append(const char *logline, int len)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if(currentBuffer_->avail() > len)  //当前buffer可写长度大于写入长度
    {
        currentBuffer_->append(logline, len);
    }else{
        buffers_.push_back(std::move(currentBuffer_)); //将当前写入的buffer写入buffers
        if(nextBuffer_)              //nextbuffer存在的话补充
        {
            currentBuffer_ = std::move(nextBuffer_);
        }else{
            currentBuffer_.reset(new Buffer);     // Rarely happens  //不存在的话重新申请
        }
        currentBuffer_->append(logline, len);   //经过上面操作确保currentBuffer_的存在
        cond_.notify_one();                 //条件变量通知
    }
}

void AsyncLogging::threadFunc()
{
    latch_.set_value();
    LogFile output(basename, rollSize, false);
    BufferPtr newBuffer1(new Buffer);
    BufferPtr newBuffer2(new Buffer);
    newBuffer1->bzero();
    newBuffer2->bzero();
    BufferVector WriteLog;
    WriteLog.reserve(16);
    auto interval = std::chrono::seconds(flushInterval_);

    while(running_)
    {
        assert(newBuffer1 && newBuffer1->length() == 0);
        assert(newBuffer2 && newBuffer2->length() == 0);
        assert(WriteLog.empty());

        {
            std::unique_lock<std::mutex> guard(mutex_);
            if(buffers_.empty())
            {
                cond_.wait_for(guard, interval);
            }
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newBuffer1);
            WriteLog.swap(buffers_);
            if(!nextBuffer_)
            {
                nextBuffer_ = std::move(newBuffer2);
            }
        }

        if(WriteLog.size() > 25)      //日志堆积 即生产者生产速度大于消费者消费速度 进行丢弃日志
        {
            char buf[256];
            snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
                        Timestamp::now().toFormattedString().c_str(),
                        WriteLog.size()-2);
            fputs(buf, stderr);
            output.append(buf, static_cast<int>(strlen(buf)));    //将超出信息写入output
            WriteLog.erase(WriteLog.begin()+2, WriteLog.end());  //留下两个缓冲块
        }

        for(const auto & buffer : WriteLog)
        {
            output.append(buffer->data(), buffer->length());
        }

        if(WriteLog.size() > 2)
        {
            WriteLog.resize(2);
        }

        if(!newBuffer1)
        {
            assert(!WriteLog.empty());
            newBuffer1 = std::move(WriteLog.back());
            WriteLog.pop_back();
            newBuffer1->reset();
        }

        if(!newBuffer2)
        {
            assert(!WriteLog.empty());
            newBuffer2 = std::move(WriteLog.back());
            WriteLog.pop_back();
            newBuffer2->reset();
        }

        WriteLog.clear();
        output.flush();//刷新logfile中的输出 即存储回磁盘
    }
    output.flush();//线程终止后转储磁盘
}