//////////////////////////////////////////////////////////////////////////////////////////
// A cross platform socket APIs, support ios & android & wp8 & window store
// universal app version: 3.9.7
//////////////////////////////////////////////////////////////////////////////////////////
/*
The MIT License (MIT)

Copyright (c) 2012-2019 halx99

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "lyasio.h"
#include "yasio_cclua.h"
#include "object_pool.h"
#include "ref_ptr.h"
#include "string_view.hpp"
#include "cocos2d.h"
using namespace cocos2d;

namespace lyasio
{
#define DEFINE_REFERENCE_CLASS                                                                     \
private:                                                                                           \
  unsigned int referenceCount_;                                                                    \
                                                                                                   \
public:                                                                                            \
  void retain() { ++referenceCount_; }                                                             \
  void release()                                                                                   \
  {                                                                                                \
    --referenceCount_;                                                                             \
    if (referenceCount_ == 0)                                                                      \
      delete this;                                                                                 \
  }                                                                                                \
                                                                                                   \
private:

namespace simple_timer
{
typedef void *TIMER_ID;
typedef std::function<void(void)> vcallback_t;
struct TimerObject
{
  TimerObject(vcallback_t &&callback) : callback_(std::move(callback)), referenceCount_(1) {}

  vcallback_t callback_;
  static uintptr_t s_timerId;

  DEFINE_OBJECT_POOL_ALLOCATION(TimerObject, 128)
  DEFINE_REFERENCE_CLASS
};

uintptr_t TimerObject::s_timerId = 0;

TIMER_ID loop(unsigned int n, float interval, vcallback_t callback)
{
  if (n > 0 && interval >= 0)
  {
    purelib::gc::ref_ptr<TimerObject> timerObj(new TimerObject(std::move(callback)));

    auto timerId = reinterpret_cast<TIMER_ID>(++TimerObject::s_timerId);

    std::string key = StringUtils::format("SIMPLE_TIMER_%p", timerId);

    Director::getInstance()->getScheduler()->schedule(
        [timerObj](
            float /*dt*/) { // lambda expression hold the reference of timerObj automatically.
          timerObj->callback_();
        },
        timerId, interval, n - 1, 0, false, key);

    return timerId;
  }
  return nullptr;
}

TIMER_ID delay(float delay, vcallback_t callback)
{
  if (delay > 0)
  {
    purelib::gc::ref_ptr<TimerObject> timerObj(new TimerObject(std::move(callback)));
    auto timerId = reinterpret_cast<TIMER_ID>(++TimerObject::s_timerId);

    std::string key = StringUtils::format("SIMPLE_TIMER_%p", timerId);
    Director::getInstance()->getScheduler()->schedule(
        [timerObj](
            float /*dt*/) { // lambda expression hold the reference of timerObj automatically.
          timerObj->callback_();
        },
        timerId, 0, 0, delay, false, key);

    return timerId;
  }
  return nullptr;
}

void kill(TIMER_ID timerId)
{
  std::string key = StringUtils::format("SIMPLE_TIMER_%p", timerId);
  Director::getInstance()->getScheduler()->unschedule(key, timerId);
}
} // namespace simple_timer
} // namespace lyasio

#if _HAS_CXX17_FULL_FEATURES

#  include "sol.hpp"

extern "C" {
struct lua_State;
YASIO_API int luaopen_yasio_cclua(lua_State *L)
{
  int n = luaopen_yasio(L);
  sol::stack_table yasio(L, 1);
  yasio.set_function("delay", lyasio::simple_timer::delay);
  yasio.set_function("loop", lyasio::simple_timer::loop);
  yasio.set_function("kill", lyasio::simple_timer::kill);
  return n;
}
}

#else

#  include "kaguya.hpp"

extern "C" {
struct lua_State;
YASIO_API int luaopen_yasio_cclua(lua_State *L)
{
  int n = luaopen_yasio(L);

  kaguya::State state(L);
  auto yasio = state.popFromStack();
  yasio.setField("delay", lyasio::simple_timer::delay);
  yasio.setField("loop", lyasio::simple_timer::loop);
  yasio.setField("kill", lyasio::simple_timer::kill);
  state.pushToStack(yasio);

  return n;
}
}
#endif
