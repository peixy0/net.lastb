#pragma once
#include <deque>
#include <string>
#include <unordered_map>
#include <variant>
#include "network.hpp"

namespace network {

class TcpSendBuffer {
public:
  TcpSendBuffer(int, std::string_view);
  TcpSendBuffer(TcpSendBuffer&) = delete;
  TcpSendBuffer(TcpSendBuffer&&) = default;
  TcpSendBuffer& operator=(TcpSendBuffer&) = delete;
  TcpSendBuffer& operator=(TcpSendBuffer&&) = default;
  ~TcpSendBuffer() = default;
  void Send();
  bool Done() const;

private:
  int peer;
  std::string buffer;
  size_t size{0};
};

class TcpSendFile {
public:
  TcpSendFile(int, os::File);
  TcpSendFile(TcpSendFile&) = delete;
  TcpSendFile(TcpSendFile&&) = default;
  TcpSendFile& operator=(TcpSendFile&) = delete;
  TcpSendFile& operator=(TcpSendFile&&) = default;
  ~TcpSendFile() = default;
  void Send();
  bool Done() const;

private:
  int peer;
  os::File file;
  size_t size{0};
};

using TcpSendOperation = std::variant<TcpSendBuffer, TcpSendFile>;

class ConcreteTcpSender : public TcpSender {
public:
  ConcreteTcpSender(int, TcpSenderSupervisor&);
  ConcreteTcpSender(const ConcreteTcpSender&) = delete;
  ConcreteTcpSender(ConcreteTcpSender&&) = delete;
  ConcreteTcpSender& operator=(const ConcreteTcpSender&) = delete;
  ConcreteTcpSender& operator=(ConcreteTcpSender&&) = delete;
  ~ConcreteTcpSender();

  void Send(std::string_view) override;
  void Send(os::File) override;
  void SendBuffered() override;
  void Close() override;

private:
  void MarkPending();
  void UnmarkPending();

  int peer;
  TcpSenderSupervisor& supervisor;
  std::deque<TcpSendOperation> buffered;
  bool pending{false};
  std::mutex senderMut;
};

class TcpConnectionContext {
public:
  TcpConnectionContext(int, std::unique_ptr<TcpProcessor>, std::unique_ptr<TcpSender>);
  TcpConnectionContext(const TcpConnectionContext&) = delete;
  TcpConnectionContext(TcpConnectionContext&&) = delete;
  TcpConnectionContext& operator=(const TcpConnectionContext&) = delete;
  TcpConnectionContext& operator=(TcpConnectionContext&&) = delete;
  ~TcpConnectionContext();

  TcpProcessor& GetProcessor();
  TcpSender& GetSender();

private:
  int fd;
  std::unique_ptr<TcpProcessor> processor;
  std::unique_ptr<TcpSender> sender;
};

class TcpLayer : public TcpSenderSupervisor {
public:
  explicit TcpLayer(std::unique_ptr<TcpProcessorFactory>);
  TcpLayer(const TcpLayer&) = delete;
  TcpLayer(TcpLayer&&) = delete;
  TcpLayer& operator=(const TcpLayer&) = delete;
  TcpLayer& operator=(TcpLayer&&) = delete;
  virtual ~TcpLayer() override;

  void Start();
  void MarkSenderPending(int) override;
  void UnmarkSenderPending(int) override;

protected:
  virtual int CreateSocket() = 0;
  void SetNonBlocking(int);

private:
  void StartLoop();
  void MarkReceiverPending(int);
  void SetupPeer();
  void ClosePeer(int);
  void ReadFromPeer(int);
  void SendToPeer(int);
  void PurgeExpiredConnections();
  int FindMostRecentTimeout() const;

  std::unique_ptr<TcpProcessorFactory> processorFactory;
  int localDescriptor{-1};
  int epollDescriptor{-1};
  std::unordered_map<int, TcpConnectionContext> connections;
};

class Tcp4Layer : public TcpLayer {
public:
  Tcp4Layer(std::string_view, std::uint16_t, std::unique_ptr<TcpProcessorFactory>);

protected:
  int CreateSocket() override;

private:
  std::string host;
  std::uint16_t port;
};

}  // namespace network
