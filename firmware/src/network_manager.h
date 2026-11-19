#pragma once

class NetworkManager {
public:
  void begin();
  void maintain();
  bool connected() const;

private:
  unsigned long lastAttempt_ = 0;
  bool mdnsStarted_ = false;
  bool timeConfigured_ = false;
};