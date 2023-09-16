#pragma once

#include <mutex>
#include <queue>
#include <condition_variable>

class MutexQueue {
public:
  MutexQueue() = default;
  void push(int x) {
    std::unique_lock<std::mutex> lk(mutex_);
    queue_.push(x);
    cond_var_.notify_all();
  }
  int pop() {
    std::unique_lock<std::mutex> lk(mutex_);
    cond_var_.wait(lk, [this] { return !queue_.empty() || finished.load(); });
    if(finished) return -1;
    int res = queue_.front();
    queue_.pop();
    return res;
  }
  bool empty() {
    std::lock_guard<std::mutex> lk(mutex_);
    return queue_.empty();
  }
  void kill() {
    std::cout << "kill" << std::endl;
    if(finished.load() == true) return;
    finished.store(true);
    cond_var_.notify_all();
  }
private:
  mutable std::mutex mutex_;
  std::condition_variable cond_var_;
  std::atomic<bool> finished = {false};
  std::queue<int> queue_; 
};