// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "wpi/DataLog.h"

#include "wpi/Synchronization.h"

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>  // NOLINT(build/include_order)

#endif

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#include <fmt/format.h>

#include "wpi/Endian.h"
#include "wpi/Logger.h"
#include "wpi/MathExtras.h"
#include "wpi/fs.h"
#include "wpi/timestamp.h"

using namespace wpi::log;

static constexpr size_t kBlockSize = 16 * 1024;
static constexpr size_t kMaxBufferCount = 1024 * 1024 / kBlockSize;
static constexpr size_t kMaxFreeCount = 256 * 1024 / kBlockSize;
static constexpr size_t kRecordMaxHeaderSize = 17;
static constexpr uintmax_t kMinFreeSpace = 5 * 1024 * 1024;

static std::string FormatBytesSize(uintmax_t value) {
  static constexpr uintmax_t kKiB = 1024;
  static constexpr uintmax_t kMiB = kKiB * 1024;
  static constexpr uintmax_t kGiB = kMiB * 1024;
  if (value >= kGiB) {
    return fmt::format("{:.1f} GiB", static_cast<double>(value) / kGiB);
  } else if (value >= kMiB) {
    return fmt::format("{:.1f} MiB", static_cast<double>(value) / kMiB);
  } else if (value >= kKiB) {
    return fmt::format("{:.1f} KiB", static_cast<double>(value) / kKiB);
  } else {
    return fmt::format("{} B", value);
  }
}

template <typename T>
static unsigned int WriteVarInt(uint8_t* buf, T val) {
  unsigned int len = 0;
  do {
    *buf++ = static_cast<unsigned int>(val) & 0xff;
    ++len;
    val >>= 8;
  } while (val != 0);
  return len;
}

// min size: 4, max size: 17
static unsigned int WriteRecordHeader(uint8_t* buf, uint32_t entry,
                                      uint64_t timestamp,
                                      uint32_t payloadSize) {
  uint8_t* origbuf = buf++;

  unsigned int entryLen = WriteVarInt(buf, entry);
  buf += entryLen;
  unsigned int payloadLen = WriteVarInt(buf, payloadSize);
  buf += payloadLen;
  unsigned int timestampLen =
      WriteVarInt(buf, timestamp == 0 ? wpi::Now() : timestamp);
  buf += timestampLen;
  *origbuf =
      ((timestampLen - 1) << 4) | ((payloadLen - 1) << 2) | (entryLen - 1);
  return buf - origbuf;
}

class DataLog::Buffer {
 public:
  explicit Buffer(size_t alloc = kBlockSize)
      : m_buf{new uint8_t[alloc]}, m_maxLen{alloc} {}
  ~Buffer() { delete[] m_buf; }

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  Buffer(Buffer&& oth)
      : m_buf{oth.m_buf}, m_len{oth.m_len}, m_maxLen{oth.m_maxLen} {
    oth.m_buf = nullptr;
    oth.m_len = 0;
    oth.m_maxLen = 0;
  }

  Buffer& operator=(Buffer&& oth) {
    if (m_buf) {
      delete[] m_buf;
    }
    m_buf = oth.m_buf;
    m_len = oth.m_len;
    m_maxLen = oth.m_maxLen;
    oth.m_buf = nullptr;
    oth.m_len = 0;
    oth.m_maxLen = 0;
    return *this;
  }

  uint8_t* Reserve(size_t size) {
    assert(size <= GetRemaining());
    uint8_t* rv = m_buf + m_len;
    m_len += size;
    return rv;
  }

  void Unreserve(size_t size) { m_len -= size; }

  void Clear() { m_len = 0; }

  size_t GetRemaining() const { return m_maxLen - m_len; }

  std::span<uint8_t> GetData() { return {m_buf, m_len}; }
  std::span<const uint8_t> GetData() const { return {m_buf, m_len}; }

 private:
  uint8_t* m_buf;
  size_t m_len = 0;
  size_t m_maxLen;
};

static void DefaultLog(unsigned int level, const char* file, unsigned int line,
                       const char* msg) {
  if (level > wpi::WPI_LOG_INFO) {
    fmt::print(stderr, "DataLog: {}\n", msg);
  } else if (level == wpi::WPI_LOG_INFO) {
    fmt::print("DataLog: {}\n", msg);
  }
}

static wpi::Logger defaultMessageLog{DefaultLog};

DataLog::DataLog(std::string_view dir, std::string_view filename, double period,
                 std::string_view extraHeader)
    : DataLog{defaultMessageLog, dir, filename, period, extraHeader} {}

DataLog::DataLog(wpi::Logger& msglog, std::string_view dir,
                 std::string_view filename, double period,
                 std::string_view extraHeader)
    : m_msglog{msglog},
      m_period{period},
      m_extraHeader{extraHeader},
      m_newFilename{filename},
      m_thread{[this, dir = std::string{dir}] { WriterThreadMain(dir); }} {}

DataLog::DataLog(std::function<void(std::span<const uint8_t> data)> write,
                 double period, std::string_view extraHeader)
    : DataLog{defaultMessageLog, std::move(write), period, extraHeader} {}

DataLog::DataLog(wpi::Logger& msglog,
                 std::function<void(std::span<const uint8_t> data)> write,
                 double period, std::string_view extraHeader)
    : m_msglog{msglog},
      m_period{period},
      m_extraHeader{extraHeader},
      m_thread{[this, write = std::move(write)] {
        WriterThreadMain(std::move(write));
      }} {}

DataLog::~DataLog() {
  {
    std::scoped_lock lock{m_mutex};
    m_active = false;
    m_doFlush = true;
  }
  m_cond.notify_all();
  m_thread.join();
}

void DataLog::SetFilename(std::string_view filename) {
  {
    std::scoped_lock lock{m_mutex};
    m_newFilename = filename;
  }
  m_cond.notify_all();
}

void DataLog::Flush() {
  {
    std::scoped_lock lock{m_mutex};
    m_doFlush = true;
  }
  m_cond.notify_all();
}

void DataLog::Pause() {
  std::scoped_lock lock{m_mutex};
  m_paused = true;
}

void DataLog::Resume() {
  std::scoped_lock lock{m_mutex};
  m_paused = false;
}

static void WriteToFile(fs::file_t f, std::span<const uint8_t> data,
                        std::string_view filename, wpi::Logger& msglog) {
  do {
#ifdef _WIN32
    DWORD ret;
    if (!WriteFile(f, data.data(), data.size(), &ret, nullptr)) {
      WPI_ERROR(msglog, "Error writing to log file '{}': {}", filename,
                GetLastError());
      break;
    }
#else
    ssize_t ret = ::write(f, data.data(), data.size());
    if (ret < 0) {
      // If it's a recoverable error, swallow it and retry the write
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }

      // Otherwise it's a non-recoverable error; quit trying
      WPI_ERROR(msglog, "Error writing to log file '{}': {}", filename,
                std::strerror(errno));
      break;
    }
#endif

    // The write may have written some or all of the data
    data = data.subspan(ret);
  } while (data.size() > 0);
}

static std::string MakeRandomFilename() {
  // build random filename
  static std::random_device dev;
  static std::mt19937 rng(dev());
  std::uniform_int_distribution<int> dist(0, 15);
  const char* v = "0123456789abcdef";
  std::string filename = "wpilog_";
  for (int i = 0; i < 16; i++) {
    filename += v[dist(rng)];
  }
  filename += ".wpilog";
  return filename;
}

void DataLog::WriterThreadMain(std::string_view dir) {
  std::chrono::duration<double> periodTime{m_period};

  std::error_code ec;
  fs::path dirPath{dir};
  std::string filename;

  {
    std::scoped_lock lock{m_mutex};
    filename = std::move(m_newFilename);
    m_newFilename.clear();
  }

  if (filename.empty()) {
    filename = MakeRandomFilename();
  }

  fs::file_t f = fs::kInvalidFile;

  // get free space
  uintmax_t freeSpace = fs::space(dirPath).available;
  if (freeSpace < kMinFreeSpace) {
    WPI_ERROR(m_msglog,
              "Insufficient free space ({} available), no log being saved",
              FormatBytesSize(freeSpace));
  } else {
    // try preferred filename, or randomize it a few times, before giving up
    for (int i = 0; i < 5; ++i) {
      // open file for append
#ifdef _WIN32
      // WIN32 doesn't allow combination of CreateNew and Append
      f = fs::OpenFileForWrite(dirPath / filename, ec, fs::CD_CreateNew,
                               fs::OF_None);
#else
      f = fs::OpenFileForWrite(dirPath / filename, ec, fs::CD_CreateNew,
                               fs::OF_Append);
#endif
      if (ec) {
        WPI_ERROR(m_msglog, "Could not open log file '{}': {}",
                  (dirPath / filename).string(), ec.message());
        // try again with random filename
        filename = MakeRandomFilename();
      } else {
        break;
      }
    }

    if (f == fs::kInvalidFile) {
      WPI_ERROR(m_msglog, "Could not open log file, no log being saved");
    } else {
      WPI_INFO(m_msglog, "Logging to '{}' ({} free space)",
               (dirPath / filename).string(), FormatBytesSize(freeSpace));
    }
  }

  // write header (version 1.0)
  if (f != fs::kInvalidFile) {
    const uint8_t header[] = {'W', 'P', 'I', 'L', 'O', 'G', 0, 1};
    WriteToFile(f, header, filename, m_msglog);
    uint8_t extraLen[4];
    support::endian::write32le(extraLen, m_extraHeader.size());
    WriteToFile(f, extraLen, filename, m_msglog);
    if (m_extraHeader.size() > 0) {
      WriteToFile(f,
                  {reinterpret_cast<const uint8_t*>(m_extraHeader.data()),
                   m_extraHeader.size()},
                  filename, m_msglog);
    }
  }

  std::vector<Buffer> toWrite;
  int freeSpaceCount = 0;
  bool blocked = false;

  std::unique_lock lock{m_mutex};
  while (m_active) {
    bool doFlush = false;
    auto timeoutTime = std::chrono::steady_clock::now() + periodTime;
    if (m_cond.wait_until(lock, timeoutTime) == std::cv_status::timeout) {
      doFlush = true;
    }

    if (!m_newFilename.empty() && f != fs::kInvalidFile) {
      auto newFilename = std::move(m_newFilename);
      m_newFilename.clear();
      lock.unlock();
      // rename
      if (filename != newFilename) {
        fs::rename(dirPath / filename, dirPath / newFilename, ec);
      }
      if (ec) {
        WPI_ERROR(m_msglog, "Could not rename log file from '{}' to '{}': {}",
                  filename, newFilename, ec.message());
      } else {
        WPI_INFO(m_msglog, "Renamed log file from '{}' to '{}'", filename,
                 newFilename);
      }
      filename = std::move(newFilename);
      lock.lock();
    }

    if (doFlush || m_doFlush) {
      // flush to file
      m_doFlush = false;
      if (m_outgoing.empty()) {
        continue;
      }
      // swap outgoing with empty vector
      toWrite.swap(m_outgoing);

      if (f != fs::kInvalidFile && !blocked) {
        lock.unlock();

        // update free space every 10 flushes (in case other things are writing)
        if (++freeSpaceCount >= 10) {
          freeSpaceCount = 0;
          freeSpace = fs::space(dirPath).available;
        }

        // write buffers to file
        for (auto&& buf : toWrite) {
          // stop writing when we go below the minimum free space
          freeSpace -= buf.GetData().size();
          if (freeSpace < kMinFreeSpace) {
            [[unlikely]] WPI_ERROR(
                m_msglog,
                "Stopped logging due to low free space ({} available)",
                FormatBytesSize(freeSpace));
            blocked = true;
            break;
          }
          WriteToFile(f, buf.GetData(), filename, m_msglog);
        }

        // sync to storage
#if defined(__linux__)
        ::fdatasync(f);
#elif defined(__APPLE__)
        ::fsync(f);
#endif
        lock.lock();
        if (blocked) {
          [[unlikely]] m_paused = true;
        }
      }

      // release buffers back to free list
      for (auto&& buf : toWrite) {
        buf.Clear();
        if (m_free.size() < kMaxFreeCount) {
          [[likely]] m_free.emplace_back(std::move(buf));
        }
      }
      toWrite.resize(0);
    }
  }

  if (f != fs::kInvalidFile) {
    fs::CloseFile(f);
  }
}

void DataLog::WriterThreadMain(
    std::function<void(std::span<const uint8_t> data)> write) {
  std::chrono::duration<double> periodTime{m_period};

  // write header (version 1.0)
  {
    const uint8_t header[] = {'W', 'P', 'I', 'L', 'O', 'G', 0, 1};
    write(header);
    uint8_t extraLen[4];
    support::endian::write32le(extraLen, m_extraHeader.size());
    write(extraLen);
    if (m_extraHeader.size() > 0) {
      write({reinterpret_cast<const uint8_t*>(m_extraHeader.data()),
             m_extraHeader.size()});
    }
  }

  std::vector<Buffer> toWrite;

  std::unique_lock lock{m_mutex};
  while (m_active) {
    bool doFlush = false;
    auto timeoutTime = std::chrono::steady_clock::now() + periodTime;
    if (m_cond.wait_until(lock, timeoutTime) == std::cv_status::timeout) {
      doFlush = true;
    }

    if (doFlush || m_doFlush) {
      // flush to file
      m_doFlush = false;
      if (m_outgoing.empty()) {
        continue;
      }
      // swap outgoing with empty vector
      toWrite.swap(m_outgoing);

      lock.unlock();
      // write buffers
      for (auto&& buf : toWrite) {
        if (!buf.GetData().empty()) {
          write(buf.GetData());
        }
      }
      lock.lock();

      // release buffers back to free list
      for (auto&& buf : toWrite) {
        buf.Clear();
        if (m_free.size() < kMaxFreeCount) {
          [[likely]] m_free.emplace_back(std::move(buf));
        }
      }
      toWrite.resize(0);
    }
  }

  write({});  // indicate EOF
}

// Control records use the following format:
// 1-byte type
// 4-byte entry
// rest of data (depending on type)

int DataLog::Start(std::string_view name, std::string_view type,
                   std::string_view metadata, int64_t timestamp) {
  std::scoped_lock lock{m_mutex};
  auto& entryInfo = m_entries[name];
  if (entryInfo.id == 0) {
    entryInfo.id = ++m_lastId;
  }
  auto& savedCount = m_entryCounts[entryInfo.id];
  ++savedCount;
  if (savedCount > 1) {
    if (entryInfo.type != type) {
      WPI_ERROR(m_msglog,
                "type mismatch for '{}': was '{}', requested '{}'; ignoring",
                name, entryInfo.type, type);
      return 0;
    }
    return entryInfo.id;
  }
  entryInfo.type = type;
  size_t strsize = name.size() + type.size() + metadata.size();
  uint8_t* buf = StartRecord(0, timestamp, 5 + 12 + strsize, 5);
  *buf++ = impl::kControlStart;
  wpi::support::endian::write32le(buf, entryInfo.id);
  AppendStringImpl(name);
  AppendStringImpl(type);
  AppendStringImpl(metadata);

  return entryInfo.id;
}

void DataLog::Finish(int entry, int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  std::scoped_lock lock{m_mutex};
  auto& savedCount = m_entryCounts[entry];
  if (savedCount == 0) {
    return;
  }
  --savedCount;
  if (savedCount != 0) {
    return;
  }
  m_entryCounts.erase(entry);
  uint8_t* buf = StartRecord(0, timestamp, 5, 5);
  *buf++ = impl::kControlFinish;
  wpi::support::endian::write32le(buf, entry);
}

void DataLog::SetMetadata(int entry, std::string_view metadata,
                          int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  std::scoped_lock lock{m_mutex};
  uint8_t* buf = StartRecord(0, timestamp, 5 + 4 + metadata.size(), 5);
  *buf++ = impl::kControlSetMetadata;
  wpi::support::endian::write32le(buf, entry);
  AppendStringImpl(metadata);
}

uint8_t* DataLog::Reserve(size_t size) {
  assert(size <= kBlockSize);
  if (m_outgoing.empty() || size > m_outgoing.back().GetRemaining()) {
    if (m_free.empty()) {
      if (m_outgoing.size() >= kMaxBufferCount) {
        [[unlikely]] WPI_ERROR(
            m_msglog,
            "outgoing buffers exceeded threshold, pausing logging--"
            "consider flushing to disk more frequently (smaller period)");
        m_paused = true;
      }
      m_outgoing.emplace_back();
    } else {
      m_outgoing.emplace_back(std::move(m_free.back()));
      m_free.pop_back();
    }
  }
  return m_outgoing.back().Reserve(size);
}

uint8_t* DataLog::StartRecord(uint32_t entry, uint64_t timestamp,
                              uint32_t payloadSize, size_t reserveSize) {
  uint8_t* buf = Reserve(kRecordMaxHeaderSize + reserveSize);
  auto headerLen = WriteRecordHeader(buf, entry, timestamp, payloadSize);
  m_outgoing.back().Unreserve(kRecordMaxHeaderSize - headerLen);
  buf += headerLen;
  return buf;
}

void DataLog::AppendImpl(std::span<const uint8_t> data) {
  while (data.size() > kBlockSize) {
    uint8_t* buf = Reserve(kBlockSize);
    std::memcpy(buf, data.data(), kBlockSize);
    data = data.subspan(kBlockSize);
  }
  uint8_t* buf = Reserve(data.size());
  std::memcpy(buf, data.data(), data.size());
}

void DataLog::AppendStringImpl(std::string_view str) {
  uint8_t* buf = Reserve(4);
  wpi::support::endian::write32le(buf, str.size());
  AppendImpl({reinterpret_cast<const uint8_t*>(str.data()), str.size()});
}

void DataLog::AppendRaw(int entry, std::span<const uint8_t> data,
                        int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  std::scoped_lock lock{m_mutex};
  if (m_paused) {
    return;
  }
  StartRecord(entry, timestamp, data.size(), 0);
  AppendImpl(data);
}

void DataLog::AppendRaw2(int entry,
                         std::span<const std::span<const uint8_t>> data,
                         int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  std::scoped_lock lock{m_mutex};
  if (m_paused) {
    return;
  }
  size_t size = 0;
  for (auto&& chunk : data) {
    size += chunk.size();
  }
  StartRecord(entry, timestamp, size, 0);
  for (auto chunk : data) {
    AppendImpl(chunk);
  }
}

void DataLog::AppendBoolean(int entry, bool value, int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  std::scoped_lock lock{m_mutex};
  if (m_paused) {
    return;
  }
  uint8_t* buf = StartRecord(entry, timestamp, 1, 1);
  buf[0] = value ? 1 : 0;
}

void DataLog::AppendInteger(int entry, int64_t value, int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  std::scoped_lock lock{m_mutex};
  if (m_paused) {
    return;
  }
  uint8_t* buf = StartRecord(entry, timestamp, 8, 8);
  wpi::support::endian::write64le(buf, value);
}

void DataLog::AppendFloat(int entry, float value, int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  std::scoped_lock lock{m_mutex};
  if (m_paused) {
    return;
  }
  uint8_t* buf = StartRecord(entry, timestamp, 4, 4);
  if constexpr (wpi::support::endian::system_endianness() ==
                wpi::support::little) {
    std::memcpy(buf, &value, 4);
  } else {
    wpi::support::endian::write32le(buf, wpi::bit_cast<uint32_t>(value));
  }
}

void DataLog::AppendDouble(int entry, double value, int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  std::scoped_lock lock{m_mutex};
  if (m_paused) {
    return;
  }
  uint8_t* buf = StartRecord(entry, timestamp, 8, 8);
  if constexpr (wpi::support::endian::system_endianness() ==
                wpi::support::little) {
    std::memcpy(buf, &value, 8);
  } else {
    wpi::support::endian::write64le(buf, wpi::bit_cast<uint64_t>(value));
  }
}

void DataLog::AppendString(int entry, std::string_view value,
                           int64_t timestamp) {
  AppendRaw(entry,
            {reinterpret_cast<const uint8_t*>(value.data()), value.size()},
            timestamp);
}

void DataLog::AppendBooleanArray(int entry, std::span<const bool> arr,
                                 int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  std::scoped_lock lock{m_mutex};
  if (m_paused) {
    return;
  }
  StartRecord(entry, timestamp, arr.size(), 0);
  uint8_t* buf;
  while (arr.size() > kBlockSize) {
    buf = Reserve(kBlockSize);
    for (auto val : arr.subspan(0, kBlockSize)) {
      *buf++ = val ? 1 : 0;
    }
    arr = arr.subspan(kBlockSize);
  }
  buf = Reserve(arr.size());
  for (auto val : arr) {
    *buf++ = val ? 1 : 0;
  }
}

void DataLog::AppendBooleanArray(int entry, std::span<const int> arr,
                                 int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  std::scoped_lock lock{m_mutex};
  if (m_paused) {
    return;
  }
  StartRecord(entry, timestamp, arr.size(), 0);
  uint8_t* buf;
  while (arr.size() > kBlockSize) {
    buf = Reserve(kBlockSize);
    for (auto val : arr.subspan(0, kBlockSize)) {
      *buf++ = val & 1;
    }
    arr = arr.subspan(kBlockSize);
  }
  buf = Reserve(arr.size());
  for (auto val : arr) {
    *buf++ = val & 1;
  }
}

void DataLog::AppendBooleanArray(int entry, std::span<const uint8_t> arr,
                                 int64_t timestamp) {
  AppendRaw(entry, arr, timestamp);
}

void DataLog::AppendIntegerArray(int entry, std::span<const int64_t> arr,
                                 int64_t timestamp) {
  if constexpr (wpi::support::endian::system_endianness() ==
                wpi::support::little) {
    AppendRaw(entry,
              {reinterpret_cast<const uint8_t*>(arr.data()), arr.size() * 8},
              timestamp);
  } else {
    if (entry <= 0) {
      return;
    }
    std::scoped_lock lock{m_mutex};
    if (m_paused) {
      return;
    }
    StartRecord(entry, timestamp, arr.size() * 8, 0);
    uint8_t* buf;
    while ((arr.size() * 8) > kBlockSize) {
      buf = Reserve(kBlockSize);
      for (auto val : arr.subspan(0, kBlockSize / 8)) {
        wpi::support::endian::write64le(buf, val);
        buf += 8;
      }
      arr = arr.subspan(kBlockSize / 8);
    }
    buf = Reserve(arr.size() * 8);
    for (auto val : arr) {
      wpi::support::endian::write64le(buf, val);
      buf += 8;
    }
  }
}

void DataLog::AppendFloatArray(int entry, std::span<const float> arr,
                               int64_t timestamp) {
  if constexpr (wpi::support::endian::system_endianness() ==
                wpi::support::little) {
    AppendRaw(entry,
              {reinterpret_cast<const uint8_t*>(arr.data()), arr.size() * 4},
              timestamp);
  } else {
    if (entry <= 0) {
      return;
    }
    std::scoped_lock lock{m_mutex};
    if (m_paused) {
      return;
    }
    StartRecord(entry, timestamp, arr.size() * 4, 0);
    uint8_t* buf;
    while ((arr.size() * 4) > kBlockSize) {
      buf = Reserve(kBlockSize);
      for (auto val : arr.subspan(0, kBlockSize / 4)) {
        wpi::support::endian::write32le(buf, wpi::bit_cast<uint32_t>(val));
        buf += 4;
      }
      arr = arr.subspan(kBlockSize / 4);
    }
    buf = Reserve(arr.size() * 4);
    for (auto val : arr) {
      wpi::support::endian::write32le(buf, wpi::bit_cast<uint32_t>(val));
      buf += 4;
    }
  }
}

void DataLog::AppendDoubleArray(int entry, std::span<const double> arr,
                                int64_t timestamp) {
  if constexpr (wpi::support::endian::system_endianness() ==
                wpi::support::little) {
    AppendRaw(entry,
              {reinterpret_cast<const uint8_t*>(arr.data()), arr.size() * 8},
              timestamp);
  } else {
    if (entry <= 0) {
      return;
    }
    std::scoped_lock lock{m_mutex};
    if (m_paused) {
      return;
    }
    StartRecord(entry, timestamp, arr.size() * 8, 0);
    uint8_t* buf;
    while ((arr.size() * 8) > kBlockSize) {
      buf = Reserve(kBlockSize);
      for (auto val : arr.subspan(0, kBlockSize / 8)) {
        wpi::support::endian::write64le(buf, wpi::bit_cast<uint64_t>(val));
        buf += 8;
      }
      arr = arr.subspan(kBlockSize / 8);
    }
    buf = Reserve(arr.size() * 8);
    for (auto val : arr) {
      wpi::support::endian::write64le(buf, wpi::bit_cast<uint64_t>(val));
      buf += 8;
    }
  }
}

void DataLog::AppendStringArray(int entry, std::span<const std::string> arr,
                                int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  // storage: 4-byte array length, each string prefixed by 4-byte length
  // calculate total size
  size_t size = 4;
  for (auto&& str : arr) {
    size += 4 + str.size();
  }
  std::scoped_lock lock{m_mutex};
  if (m_paused) {
    return;
  }
  uint8_t* buf = StartRecord(entry, timestamp, size, 4);
  wpi::support::endian::write32le(buf, arr.size());
  for (auto&& str : arr) {
    AppendStringImpl(str);
  }
}

void DataLog::AppendStringArray(int entry,
                                std::span<const std::string_view> arr,
                                int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  // storage: 4-byte array length, each string prefixed by 4-byte length
  // calculate total size
  size_t size = 4;
  for (auto&& str : arr) {
    size += 4 + str.size();
  }
  std::scoped_lock lock{m_mutex};
  if (m_paused) {
    return;
  }
  uint8_t* buf = StartRecord(entry, timestamp, size, 4);
  wpi::support::endian::write32le(buf, arr.size());
  for (auto&& sv : arr) {
    AppendStringImpl(sv);
  }
}

void DataLog::AppendStringArray(int entry,
                                std::span<const WPI_DataLog_String> arr,
                                int64_t timestamp) {
  if (entry <= 0) {
    return;
  }
  // storage: 4-byte array length, each string prefixed by 4-byte length
  // calculate total size
  size_t size = 4;
  for (auto&& str : arr) {
    size += 4 + str.len;
  }
  std::scoped_lock lock{m_mutex};
  if (m_paused) {
    return;
  }
  uint8_t* buf = StartRecord(entry, timestamp, size, 4);
  wpi::support::endian::write32le(buf, arr.size());
  for (auto&& sv : arr) {
    AppendStringImpl(sv.str);
  }
}

extern "C" {

struct WPI_DataLog* WPI_DataLog_Create(const char* dir, const char* filename,
                                       double period, const char* extraHeader) {
  return reinterpret_cast<WPI_DataLog*>(
      new DataLog{dir, filename, period, extraHeader});
}

struct WPI_DataLog* WPI_DataLog_Create_Func(
    void (*write)(void* ptr, const uint8_t* data, size_t len), void* ptr,
    double period, const char* extraHeader) {
  return reinterpret_cast<WPI_DataLog*>(
      new DataLog{[=](auto data) { write(ptr, data.data(), data.size()); },
                  period, extraHeader});
}

void WPI_DataLog_Release(struct WPI_DataLog* datalog) {
  delete reinterpret_cast<DataLog*>(datalog);
}

void WPI_DataLog_SetFilename(struct WPI_DataLog* datalog,
                             const char* filename) {
  reinterpret_cast<DataLog*>(datalog)->SetFilename(filename);
}

void WPI_DataLog_Flush(struct WPI_DataLog* datalog) {
  reinterpret_cast<DataLog*>(datalog)->Flush();
}

void WPI_DataLog_Pause(struct WPI_DataLog* datalog) {
  reinterpret_cast<DataLog*>(datalog)->Pause();
}

void WPI_DataLog_Resume(struct WPI_DataLog* datalog) {
  reinterpret_cast<DataLog*>(datalog)->Resume();
}

int WPI_DataLog_Start(struct WPI_DataLog* datalog, const char* name,
                      const char* type, const char* metadata,
                      int64_t timestamp) {
  return reinterpret_cast<DataLog*>(datalog)->Start(name, type, metadata,
                                                    timestamp);
}

void WPI_DataLog_Finish(struct WPI_DataLog* datalog, int entry,
                        int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->Finish(entry, timestamp);
}

void WPI_DataLog_SetMetadata(struct WPI_DataLog* datalog, int entry,
                             const char* metadata, int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->SetMetadata(entry, metadata, timestamp);
}

void WPI_DataLog_AppendRaw(struct WPI_DataLog* datalog, int entry,
                           const uint8_t* data, size_t len, int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendRaw(entry, {data, len}, timestamp);
}

void WPI_DataLog_AppendBoolean(struct WPI_DataLog* datalog, int entry,
                               int value, int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendBoolean(entry, value, timestamp);
}

void WPI_DataLog_AppendInteger(struct WPI_DataLog* datalog, int entry,
                               int64_t value, int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendInteger(entry, value, timestamp);
}

void WPI_DataLog_AppendFloat(struct WPI_DataLog* datalog, int entry,
                             float value, int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendFloat(entry, value, timestamp);
}

void WPI_DataLog_AppendDouble(struct WPI_DataLog* datalog, int entry,
                              double value, int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendDouble(entry, value, timestamp);
}

void WPI_DataLog_AppendString(struct WPI_DataLog* datalog, int entry,
                              const char* value, size_t len,
                              int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendString(entry, {value, len},
                                                    timestamp);
}

void WPI_DataLog_AppendBooleanArray(struct WPI_DataLog* datalog, int entry,
                                    const int* arr, size_t len,
                                    int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendBooleanArray(entry, {arr, len},
                                                          timestamp);
}

void WPI_DataLog_AppendBooleanArrayByte(struct WPI_DataLog* datalog, int entry,
                                        const uint8_t* arr, size_t len,
                                        int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendBooleanArray(entry, {arr, len},
                                                          timestamp);
}

void WPI_DataLog_AppendIntegerArray(struct WPI_DataLog* datalog, int entry,
                                    const int64_t* arr, size_t len,
                                    int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendIntegerArray(entry, {arr, len},
                                                          timestamp);
}

void WPI_DataLog_AppendFloatArray(struct WPI_DataLog* datalog, int entry,
                                  const float* arr, size_t len,
                                  int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendFloatArray(entry, {arr, len},
                                                        timestamp);
}

void WPI_DataLog_AppendDoubleArray(struct WPI_DataLog* datalog, int entry,
                                   const double* arr, size_t len,
                                   int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendDoubleArray(entry, {arr, len},
                                                         timestamp);
}

void WPI_DataLog_AppendStringArray(struct WPI_DataLog* datalog, int entry,
                                   const WPI_DataLog_String* arr, size_t len,
                                   int64_t timestamp) {
  reinterpret_cast<DataLog*>(datalog)->AppendStringArray(entry, {arr, len},
                                                         timestamp);
}

}  // extern "C"
