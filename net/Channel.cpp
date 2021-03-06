#include "Channel.h"
#include "../base/Logging.h"
#include "EventLoop.h"
#include "PollPoller.h"
#include <sstream>
#include <poll.h>
 
using namespace eff;
using namespace eff::net;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop * loop, int fd__)
    :   loop_(loop),
        fd_(fd__),
        events_(0),
        revents_(0),
        index_(-1),
        //logHup_(true),
        //tied_(false),
        eventHandling_(false),
        addedToLoop_(false)
{
    LOG_DEBUG << "new channel";
}

Channel::~Channel()
{
    // assert(!eventHandling_);
    // assert(!addedToLoop_);
    // if(loop_->isInLoopThread())
    // {
        LOG_DEBUG << "loop haschannel";
        // assert(!loop_->hasChannel(this));
    //}
    LOG_DEBUG << "end ~channel";
}

void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;
    tied_ = true;
}

void Channel::remove()
{
    //assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::update()
{
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    // std::shared_ptr<void> guard;
    // if(tied_)
    // {
    //     guard = tie_.lock();
    //     if(guard)
    //     {
    //         handleEventWithGuard(receiveTime);
    //     }
    // }else
    // {
        handleEventWithGuard(receiveTime);
    //}
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{

    eventHandling_ = true;

    LOG_TRACE << reventsToString();

    if((revents_ & POLLHUP) && !(revents_ & POLLIN))
    {
        if(logHup_)
        {
            LOG_WARN << "fd = " << fd_ << "Channel::handle_event() POLLHUP";
        }
        if(closeCallback_) closeCallback_();
    }

    if(revents_ & POLLNVAL)    //描述字不是一个打开的文件
    {
        LOG_WARN <<  "fd = " << fd_ << "Channel::handle_event() POLLNVAL";
    }

    if(revents_ & (POLLERR | POLLNVAL))    //发生错误  |  描述字不是一个打开的文件
    {
        if(errorCallback_) errorCallback_();
    }

    if(revents_ & (POLLIN | POLLPRI | POLLRDHUP))  //普通或优先级带数据可读
    {                 
                                             //高优先级数据可读
        if(readCallback_) readCallback_(receiveTime);          //发生挂起
    }

    if(revents_ & POLLOUT)                          //普通数据可写
    {
        if(writeCallback_) writeCallback_();
    }
    eventHandling_ = false;
}

std::string Channel::reventsToString() const 
{
    return eventsToString(fd_, revents_);
}

std::string Channel::eventsToString() const
{
    return eventsToString(fd_, events_);
}

std::string Channel::eventsToString(int fd, int ev)
{
    std::ostringstream oss;
    oss << fd << ": ";
    if(ev & POLLIN)
        oss << "IN ";
    if(ev & POLLPRI)
        oss << "PRI";
    if(ev & POLLOUT)
        oss << "OUT ";
    if(ev & POLLHUP)
        oss << "HUP ";
    if(ev & POLLRDHUP)
        oss << "ERR ";
    if(ev & POLLNVAL)
        oss << "NVAL ";

    return oss.str();
}
