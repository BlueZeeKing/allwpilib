// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <wpi/function_ref.h>
#include <wpinet/WebSocket.h>
#include <wpinet/uv/Buffer.h>

#include "WireConnection.h"

namespace nt::net {

class WebSocketConnection final
    : public WireConnection,
      public std::enable_shared_from_this<WebSocketConnection> {
 public:
  WebSocketConnection(wpi::WebSocket& ws, unsigned int version);
  ~WebSocketConnection() override;
  WebSocketConnection(const WebSocketConnection&) = delete;
  WebSocketConnection& operator=(const WebSocketConnection&) = delete;

  unsigned int GetVersion() const final { return m_version; }

  void SendPing(uint64_t time) final;

  bool Ready() const final { return !m_ws.IsWriteInProgress(); }

  int WriteText(wpi::function_ref<void(wpi::raw_ostream& os)> writer) final {
    return Write(kText, writer);
  }
  int WriteBinary(wpi::function_ref<void(wpi::raw_ostream& os)> writer) final {
    return Write(kBinary, writer);
  }
  int Flush() final;

  void SendText(wpi::function_ref<void(wpi::raw_ostream& os)> writer) final {
    Send(wpi::WebSocket::Frame::kText, writer);
  }
  void SendBinary(wpi::function_ref<void(wpi::raw_ostream& os)> writer) final {
    Send(wpi::WebSocket::Frame::kBinary, writer);
  }

  uint64_t GetLastFlushTime() const final { return m_lastFlushTime; }

  uint64_t GetLastPingResponse() const final { return m_lastPingResponse; }

  void Disconnect(std::string_view reason) final;

  std::string_view GetDisconnectReason() const { return m_reason; }

 private:
  enum State { kEmpty, kText, kBinary };

  int Write(State kind, wpi::function_ref<void(wpi::raw_ostream& os)> writer);
  void Send(uint8_t opcode,
            wpi::function_ref<void(wpi::raw_ostream& os)> writer);

  void StartFrame(uint8_t opcode);
  void FinishText();
  wpi::uv::Buffer AllocBuf();
  void ReleaseBufs(std::span<wpi::uv::Buffer> bufs);

  wpi::WebSocket& m_ws;

  class Stream;

  // Can't use WS frames directly as span could have dangling pointers
  struct Frame {
    Frame(uint8_t opcode, size_t start, size_t end)
        : start{start}, end{end}, opcode{opcode} {}
    size_t start;
    size_t end;
    unsigned int count = 0;
    uint8_t opcode;
  };
  std::vector<wpi::WebSocket::Frame> m_ws_frames;  // to reduce allocs
  std::vector<Frame> m_frames;
  std::vector<wpi::uv::Buffer> m_bufs;
  std::vector<wpi::uv::Buffer> m_buf_pool;
  size_t m_framePos = 0;
  size_t m_written = 0;
  wpi::uv::Error m_err;
  State m_state = kEmpty;
  std::string m_reason;
  uint64_t m_lastFlushTime = 0;
  uint64_t m_lastPingResponse = 0;
  unsigned int m_version;
};

}  // namespace nt::net
