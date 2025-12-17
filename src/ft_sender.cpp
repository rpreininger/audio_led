// ====================================================================
//  Flaschen-Taschen UDP Sender Implementation
// ====================================================================

#include "ft_sender.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>

FTSender::FTSender()
    : m_socket(-1)
    , m_destAddr(nullptr)
    , m_enabled(false)
    , m_framesSent(0)
    , m_bytesSent(0)
    , m_port(1337)
{
}

FTSender::~FTSender() {
    if (m_socket >= 0) {
        close(m_socket);
    }
    delete m_destAddr;
}

bool FTSender::init(const std::string& host, int port) {
    m_host = host;
    m_port = port;

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        std::cerr << "FTSender: Failed to create UDP socket" << std::endl;
        return false;
    }

    // Resolve hostname
    struct hostent* he = gethostbyname(host.c_str());
    if (!he) {
        std::cerr << "FTSender: Failed to resolve host: " << host << std::endl;
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Setup destination address
    m_destAddr = new struct sockaddr_in;
    memset(m_destAddr, 0, sizeof(*m_destAddr));
    m_destAddr->sin_family = AF_INET;
    m_destAddr->sin_port = htons(port);
    memcpy(&m_destAddr->sin_addr, he->h_addr_list[0], he->h_length);

    m_enabled.store(true);

    std::cerr << "FTSender: Initialized, sending to " << host << ":" << port << std::endl;
    return true;
}

void FTSender::send(const uint8_t* framebuffer, int width, int height,
                    int offsetX, int offsetY) {
    if (!m_enabled.load() || m_socket < 0 || !m_destAddr) {
        return;
    }

    // Build PPM P6 packet
    // Header format: P6\n[#FT: offsetX offsetY\n]width height\n255\n<pixel data>

    // Calculate sizes
    int pixelDataSize = width * height * 3;

    // Build header
    char header[64];
    int headerLen;

    if (offsetX != 0 || offsetY != 0) {
        headerLen = snprintf(header, sizeof(header),
                            "P6\n#FT: %d %d\n%d %d\n255\n",
                            offsetX, offsetY, width, height);
    } else {
        headerLen = snprintf(header, sizeof(header),
                            "P6\n%d %d\n255\n",
                            width, height);
    }

    // Allocate packet buffer
    int packetSize = headerLen + pixelDataSize;
    uint8_t* packet = new uint8_t[packetSize];

    // Copy header and pixel data
    memcpy(packet, header, headerLen);
    memcpy(packet + headerLen, framebuffer, pixelDataSize);

    // Send packet
    ssize_t sent = sendto(m_socket, packet, packetSize, 0,
                          (struct sockaddr*)m_destAddr, sizeof(*m_destAddr));

    delete[] packet;

    if (sent > 0) {
        m_framesSent++;
        m_bytesSent += sent;
    }
}

std::string FTSender::getBroadcastAddress() {
    struct ifaddrs* ifaddr = nullptr;
    std::string broadcastAddr;

    if (getifaddrs(&ifaddr) == -1) {
        std::cerr << "FTSender: Failed to get network interfaces" << std::endl;
        return "";
    }

    // Iterate through interfaces to find a suitable broadcast address
    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        // Only consider IPv4 interfaces
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        // Skip loopback interfaces
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;

        // Must be up and support broadcast
        if (!(ifa->ifa_flags & IFF_UP)) continue;
        if (!(ifa->ifa_flags & IFF_BROADCAST)) continue;

        // Get broadcast address
        if (ifa->ifa_broadaddr != nullptr) {
            struct sockaddr_in* bcast = (struct sockaddr_in*)ifa->ifa_broadaddr;
            char addrBuf[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &bcast->sin_addr, addrBuf, sizeof(addrBuf))) {
                broadcastAddr = addrBuf;
                std::cerr << "FTSender: Found broadcast address " << broadcastAddr
                          << " on interface " << ifa->ifa_name << std::endl;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);

    if (broadcastAddr.empty()) {
        std::cerr << "FTSender: No suitable broadcast interface found" << std::endl;
    }

    return broadcastAddr;
}

bool FTSender::initMulticast(int port, const std::string& group) {
    m_host = group;
    m_port = port;

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        std::cerr << "FTSender: Failed to create UDP socket" << std::endl;
        return false;
    }

    // Set multicast TTL (1 = local network only)
    unsigned char ttl = 1;
    if (setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        std::cerr << "FTSender: Failed to set multicast TTL" << std::endl;
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Setup destination address
    m_destAddr = new struct sockaddr_in;
    memset(m_destAddr, 0, sizeof(*m_destAddr));
    m_destAddr->sin_family = AF_INET;
    m_destAddr->sin_port = htons(port);
    inet_pton(AF_INET, group.c_str(), &m_destAddr->sin_addr);

    m_enabled.store(true);

    std::cerr << "FTSender: Multicasting to " << group << ":" << port << std::endl;
    return true;
}

bool FTSender::initBroadcast(int port) {
    std::string bcastAddr = getBroadcastAddress();
    if (bcastAddr.empty()) {
        // Fallback to global broadcast
        bcastAddr = "255.255.255.255";
        std::cerr << "FTSender: Using global broadcast " << bcastAddr << std::endl;
    }

    m_host = bcastAddr;
    m_port = port;

    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_socket < 0) {
        std::cerr << "FTSender: Failed to create UDP socket" << std::endl;
        return false;
    }

    // Enable broadcast on the socket
    int broadcastEnable = 1;
    if (setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST,
                   &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        std::cerr << "FTSender: Failed to enable broadcast on socket" << std::endl;
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // Setup destination address
    m_destAddr = new struct sockaddr_in;
    memset(m_destAddr, 0, sizeof(*m_destAddr));
    m_destAddr->sin_family = AF_INET;
    m_destAddr->sin_port = htons(port);
    inet_pton(AF_INET, bcastAddr.c_str(), &m_destAddr->sin_addr);

    m_enabled.store(true);

    std::cerr << "FTSender: Broadcasting to " << bcastAddr << ":" << port << std::endl;
    return true;
}
