#include <condition_variable>
#include <mutex>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <p2p/Event/Generator.hpp>
#include <p2p/Event/Source.hpp>
#include <p2p/Event/Timer.hpp>

#include "../Internal/IOService.hpp"
#include "SourceImpl.hpp"

namespace p2p {

	namespace Event {
	
		namespace {
			
			IOService& GetIOService() {
				static IOService ioService;
				return ioService;
			}
			
		}
		
		struct TimerImpl {
			std::mutex mutex;
			std::condition_variable condition;
			bool isRunning;
			boost::asio::deadline_timer timer;
			Generator eventGenerator;
			
			inline TimerImpl(boost::asio::io_service& pIOService) :
				isRunning(false), timer(pIOService) { }
		};
		
		namespace {
			
			void timerCallback(TimerImpl* impl, const boost::system::error_code&) {
				std::lock_guard<std::mutex> lock(impl->mutex);
				impl->eventGenerator.activate();
				impl->isRunning = false;
				impl->condition.notify_all();
			}
			
		}
		
		Timer::Timer()
			: impl_(new TimerImpl(GetIOService())) { }
			
		Timer::~Timer() {
			std::unique_lock<std::mutex> lock(impl_->mutex);
			while (impl_->isRunning) {
				impl_->condition.wait(lock);
			}
		}
		
		Timer::Timer(Timer&& other)
			: impl_(std::move(other.impl_)) { }
		
		Timer& Timer::operator=(Timer&& other) {
			std::swap(impl_, other.impl_);
			return *this;
		}
		
		void Timer::setMilliseconds(double milliseconds) {
			std::lock_guard<std::mutex> lock(impl_->mutex);
			impl_->timer.expires_from_now(boost::posix_time::milliseconds(milliseconds));
		}
		
		void Timer::schedule() {
			std::lock_guard<std::mutex> lock(impl_->mutex);
			impl_->isRunning = true;
			impl_->timer.async_wait(boost::bind(timerCallback, impl_.get(), _1));
		}
		
		bool Timer::hasExpired() const {
			std::lock_guard<std::mutex> lock(impl_->mutex);
			return !impl_->isRunning;
		}
		
		Source Timer::eventSource() const {
			return impl_->eventGenerator.eventSource();
		}
		
	}
	
}
