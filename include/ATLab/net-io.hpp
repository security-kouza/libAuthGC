#ifndef ATLAB_NET_IO_HPP
#define ATLAB_NET_IO_HPP

#include <array>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <immintrin.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <emp-tool/utils/block.h>
#include <emp-tool/utils/group.h>
#include <emp-tool/utils/prg.h>

namespace ATLab {

    constexpr int NETWORK_BUFFER_SIZE2 {1024 * 32};
    constexpr int NETWORK_BUFFER_SIZE {1024 * 1024};

    template<typename T>
    class IOChannel {
    public:
        uint64_t sentBytes {0};
        uint64_t receivedBytes {0};

        void send_data(const void* data, size_t nbyte) {
            sentBytes += nbyte;
            derived().send_data_internal(data, nbyte);
        }

        void recv_data(void* data, size_t nbyte) {
            receivedBytes += nbyte;
            derived().recv_data_internal(data, nbyte);
        }

        [[nodiscard]]
        size_t bytes_transferred() const {
            return sentBytes + receivedBytes;
        }

        void log_transferred() const {
            std::clog << derived().role << ": " << bytes_transferred() << '\n';
        }

        void send_block(const emp::block* data, size_t nblock) {
            send_data(data, nblock * sizeof(emp::block));
        }

        void recv_block(emp::block* data, size_t nblock) {
            recv_data(data, nblock * sizeof(emp::block));
        }

        void send_pt(emp::Point* points, size_t numPoints = 1) {
            for (size_t i = 0; i < numPoints; ++i) {
                size_t len {points[i].size()};
                points[i].group->resize_scratch(len);
                unsigned char* scratch {points[i].group->scratch};
                send_data(&len, 4);
                points[i].to_bin(scratch, len);
                send_data(scratch, len);
            }
        }

        void recv_pt(emp::Group* group, emp::Point* points, size_t numPoints = 1) {
            size_t len {0};
            for (size_t i = 0; i < numPoints; ++i) {
                recv_data(&len, 4);
                assert(len <= 2048);
                group->resize_scratch(len);
                unsigned char* scratch {group->scratch};
                recv_data(scratch, len);
                points[i].from_bin(group, scratch, len);
            }
        }

        void send_bool(bool* data, size_t length) {
            void* ptr {static_cast<void*>(data)};
            size_t space {length};
            const void* aligned {std::align(alignof(uint64_t), sizeof(uint64_t), ptr, space)};
            if (aligned == nullptr) {
                send_data(data, length);
            } else {
                const size_t diff {length - space};
                send_data(data, diff);
                send_bool_aligned(static_cast<const bool*>(aligned), length - diff);
            }
        }

        void recv_bool(bool* data, size_t length) {
            void* ptr {static_cast<void*>(data)};
            size_t space {length};
            void* aligned {std::align(alignof(uint64_t), sizeof(uint64_t), ptr, space)};
            if (aligned == nullptr) {
                recv_data(data, length);
            } else {
                const size_t diff {length - space};
                recv_data(data, diff);
                recv_bool_aligned(static_cast<bool*>(aligned), length - diff);
            }
        }

        void send_bool_aligned(const bool* data, size_t length) {
            const bool* data64 {data};
            size_t i {0};
            unsigned long long unpack {0};
            for (; i < length / 8; ++i) {
                unsigned long long mask {0x0101010101010101ULL};
                unsigned long long tmp {0};
                std::memcpy(&unpack, data64, sizeof(unpack));
                data64 += sizeof(unpack);
#if defined(__BMI2__)
                tmp = _pext_u64(unpack, mask);
#else
                for (unsigned long long bitMask = 1; mask != 0; bitMask += bitMask) {
                    if (unpack & mask & -mask) {
                        tmp |= bitMask;
                    }
                    mask &= (mask - 1);
                }
#endif
                send_data(&tmp, 1);
            }
            if (8 * i != length) {
                send_data(data + 8 * i, length - 8 * i);
            }
        }

        void recv_bool_aligned(bool* data, size_t length) {
            bool* data64 {data};
            size_t i {0};
            unsigned long long unpack {0};
            for (; i < length / 8; ++i) {
                unsigned long long mask {0x0101010101010101ULL};
                unsigned long long tmp {0};
                recv_data(&tmp, 1);
#if defined(__BMI2__)
                unpack = _pdep_u64(tmp, mask);
#else
                unpack = 0;
                for (unsigned long long bitMask = 1; mask != 0; bitMask += bitMask) {
                    if (tmp & bitMask) {
                        unpack |= mask & (-mask);
                    }
                    mask &= (mask - 1);
                }
#endif
                std::memcpy(data64, &unpack, sizeof(unpack));
                data64 += sizeof(unpack);
            }
            if (8 * i != length) {
                recv_data(data + 8 * i, length - 8 * i);
            }
        }

    private:
        T& derived() {
            return *static_cast<T*>(this);
        }

        const T& derived() const {
            return *static_cast<const T*>(this);
        }
    };

    class NetIO : public IOChannel<NetIO> {
    public:
        enum Role {
            CLIENT,
            SERVER
        };

        const std::string addr;
        const int port;
        const Role role;

        NetIO(const Role roleIn, const std::string& address, int portIn, bool quiet = false):
            addr {address},
            port {portIn},
            role {roleIn}
        {
            if (port < 0 || port > 65535) {
                throw std::runtime_error {"Invalid port number"};
            }

            if (role == Role::SERVER) {
                sockaddr_in dest {};
                sockaddr_in serv {};
                socklen_t sockSize {sizeof(sockaddr_in)};
                serv.sin_family = AF_INET;
                serv.sin_addr.s_addr = inet_addr(address.c_str());
                serv.sin_port = htons(port);

                mysocket = ::socket(AF_INET, SOCK_STREAM, 0);
                if (mysocket < 0) {
                    throw std::runtime_error {"Unable to create server socket"};
                }

                const int reuse {1};
                setsockopt(mysocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
                if (::bind(mysocket, reinterpret_cast<sockaddr*>(&serv), sizeof(sockaddr)) < 0) {
                    throw std::runtime_error {std::string{"bind failed: "} + std::strerror(errno)};
                }
                if (::listen(mysocket, 1) < 0) {
                    throw std::runtime_error {std::string{"listen failed: "} + std::strerror(errno)};
                }
                consocket = ::accept(mysocket, reinterpret_cast<sockaddr*>(&dest), &sockSize);
                if (consocket < 0) {
                    throw std::runtime_error {std::string{"accept failed: "} + std::strerror(errno)};
                }
                ::close(mysocket);
            } else {
                sockaddr_in dest {};
                dest.sin_family = AF_INET;
                dest.sin_addr.s_addr = inet_addr(address.c_str());
                dest.sin_port = htons(port);

                while (true) {
                    consocket = ::socket(AF_INET, SOCK_STREAM, 0);
                    if (consocket < 0) {
                        throw std::runtime_error {"Unable to create client socket"};
                    }

                    if (::connect(consocket, reinterpret_cast<sockaddr*>(&dest), sizeof(sockaddr)) == 0) {
                        break;
                    }

                    ::close(consocket);
                    usleep(1000);
                }
            }

            set_nodelay();

            sendStream = fdopen(consocket, "wb");
            const int recvFd {dup(consocket)};
            if (recvFd < 0 || sendStream == nullptr) {
                throw std::runtime_error {"fdopen failed"};
            }

            recvStream = fdopen(recvFd, "rb");
            if (recvStream == nullptr) {
                throw std::runtime_error {"fdopen failed"};
            }

            sendBuffer = new char[NETWORK_BUFFER_SIZE];
            std::memset(sendBuffer, 0, NETWORK_BUFFER_SIZE);
            setvbuf(sendStream, sendBuffer, _IOFBF, NETWORK_BUFFER_SIZE);

            recvBuffer = new char[NETWORK_BUFFER_SIZE];
            std::memset(recvBuffer, 0, NETWORK_BUFFER_SIZE);
            setvbuf(recvStream, recvBuffer, _IOFBF, NETWORK_BUFFER_SIZE);
            if (!quiet) {
                std::cout << "connected\n";
            }
        }

        ~NetIO() {
            flush();
            if (sendStream != nullptr) {
                fclose(sendStream);
            }
            if (recvStream != nullptr) {
                fclose(recvStream);
            }
            delete[] sendBuffer;
            delete[] recvBuffer;
        }

        [[nodiscard]]
        bool is_server() const {
            return role == Role::SERVER;
        }

        void sync() {
            int tmp {0};
            if (is_server()) {
                send_data_internal(&tmp, 1);
                recv_data_internal(&tmp, 1);
            } else {
                recv_data_internal(&tmp, 1);
                send_data_internal(&tmp, 1);
                flush();
            }
        }

        void set_nodelay() {
            const int one {1};
            setsockopt(consocket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        }

        void set_delay() {
            const int zero {0};
            setsockopt(consocket, IPPROTO_TCP, TCP_NODELAY, &zero, sizeof(zero));
        }

        void flush() {
            if (sendStream != nullptr) {
                fflush(sendStream);
            }
        }

        void send_data_internal(const void* data, size_t len) {
            size_t sent {0};
            while (sent < len) {
                const size_t res {fwrite(static_cast<const char*>(data) + sent, 1, len - sent, sendStream)};
                if (res > 0) {
                    sent += res;
                } else {
                    throw std::runtime_error {"net send data"};
                }
            }
            hasSent = true;
        }

        void recv_data_internal(void* data, size_t len) {
            if (hasSent && sendStream != nullptr) {
                fflush(sendStream);
            }
            hasSent = false;
            size_t received {0};
            while (received < len) {
                const size_t res {fread(static_cast<char*>(data) + received, 1, len - received, recvStream)};
                if (res > 0) {
                    received += res;
                } else {
                    throw std::runtime_error {"net recv data"};
                }
            }
        }

        [[nodiscard]]
        const std::string& address() const {
            return addr;
        }

    private:
        int mysocket {-1};
        int consocket {-1};
        FILE* sendStream {nullptr};
        FILE* recvStream {nullptr};
        char* sendBuffer {nullptr};
        char* recvBuffer {nullptr};
        bool hasSent {false};
    };
}

#endif // ATLAB_NET_IO_HPP
