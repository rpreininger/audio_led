// ====================================================================
//  Flaschen-Taschen UDP Sender
//  Sends frame data to a remote FT server using PPM P6 format over UDP
// ====================================================================

#ifndef FT_SENDER_H
#define FT_SENDER_H

#include <string>
#include <cstdint>
#include <atomic>

class FTSender {
public:
    FTSender();
    ~FTSender();

    // Initialize the sender with destination host and port
    // Returns true on success
    bool init(const std::string& host, int port = 1337);

    // Initialize with broadcast address (auto-detected)
    // Returns true on success
    bool initBroadcast(int port = 1337);

    // Get the subnet broadcast address (e.g., "192.168.1.255")
    // Returns empty string on failure
    static std::string getBroadcastAddress();

    // Send a frame to the FT server
    // framebuffer: RGB pixel data (width * height * 3 bytes)
    // width, height: dimensions of the frame
    // offsetX, offsetY: optional offset for positioning on larger displays
    void send(const uint8_t* framebuffer, int width, int height,
              int offsetX = 0, int offsetY = 0);

    // Check if sender is enabled and initialized
    bool isEnabled() const { return m_enabled.load(); }

    // Enable/disable sending
    void setEnabled(bool enabled) { m_enabled.store(enabled); }

    // Get statistics
    uint64_t getFramesSent() const { return m_framesSent.load(); }
    uint64_t getBytesSent() const { return m_bytesSent.load(); }

private:
    int m_socket;
    struct sockaddr_in* m_destAddr;
    std::atomic<bool> m_enabled;
    std::atomic<uint64_t> m_framesSent;
    std::atomic<uint64_t> m_bytesSent;
    std::string m_host;
    int m_port;
};

#endif // FT_SENDER_H
