/*
This file is part of EOTKyber of Abe-Tibouchi Laboratory, Kyoto University
Copyright © 2023-2025  Kyoto University
Copyright © 2023-2025  Peihao Li <li.peihao.62s@st.kyoto-u.ac.jp>

EOTKyber is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

EOTKyber is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "socket.hpp"

#include <thread>
#include <chrono>
#include "utils.hpp"

namespace ATLab {

    const EVP_MD* Socket::DIGEST_TYPE {EVP_sha256()};

    Socket::Socket(const std::string& address, const unsigned short port):
        _endpoint {boost::asio::ip::make_address(address), port},
        _socket {_ioc},
        _mdctx {EVP_MD_CTX_new()},
        _digestBuf {}
    {
        EVP_DigestInit_ex(_mdctx, DIGEST_TYPE, nullptr);
    }

    void Socket::accept() {
        boost::asio::ip::tcp::acceptor{_ioc, _endpoint}.accept(_socket);
        _socket.set_option(boost::asio::ip::tcp::no_delay{true});
    }

    void Socket::connect(const size_t MaxReconnectCnt) {
        size_t retryCnt {0};
        bool connected {false};
        constexpr auto baseDelayTime {std::chrono::milliseconds{5}};
        while (!connected) {
            try {
                _socket.connect(_endpoint);
                _socket.set_option(boost::asio::ip::tcp::no_delay{true});
                connected = true;
            } catch (const std::exception& e) {
                if (retryCnt >= MaxReconnectCnt) {
                    throw std::runtime_error("Failed to connect after maximum number of retries");
                }

                const auto delayTime {baseDelayTime * (1 << retryCnt)};
                std::this_thread::sleep_for(delayTime);
                ++retryCnt;
            }
        }
#ifdef DEBUG
        if (retryCnt) {
            std::cerr << "retried " << retryCnt << " times.\n";
        }
#endif // DEBUG
    }

    void Socket::close() {
        _socket.close();
    }

    size_t Socket::read(void* const pBuf, const size_t size, const std::string& logMsg, const bool debug) {
        if (debug) {
            print_log("Reading: " + logMsg);
        }
        const auto readSize {boost::asio::read(_socket, boost::asio::buffer(pBuf, size))};
        EVP_DigestUpdate(_mdctx, pBuf, readSize);
#ifdef DEBUG
        if (debug) {
            std::ostringstream sout;
            sout << readSize << " bytes read:\n";
            print_bytes(pBuf, readSize, sout);

            PrintMutex.lock();
            std::clog << sout.str();
            PrintMutex.unlock();
        }
#endif // DEBUG
        return readSize;
    }

    size_t Socket::write(const void* pBuf, const size_t size, const std::string& logMsg, const bool debug) {
        if (debug) {
            print_log("Writing: " + logMsg);
        }
        const auto writtenSize {boost::asio::write(_socket, boost::asio::buffer(pBuf, size))};
        EVP_DigestUpdate(_mdctx, pBuf, writtenSize);
#ifdef DEBUG
        if (debug) {
            std::ostringstream sout;
            sout << writtenSize << " bytes written:\n";
            print_bytes(pBuf, writtenSize, sout);

            PrintMutex.lock();
            std::clog << sout.str();
            PrintMutex.unlock();
        }
#endif // DEBUG
        return writtenSize;
    }

    std::array<std::byte, Socket::DIGEST_SIZE> Socket::gen_challenge() {
        unsigned int len;
        const auto mdctx {EVP_MD_CTX_dup(_mdctx)};

        EVP_DigestFinal_ex(mdctx, reinterpret_cast<unsigned char*>(_digestBuf.data()), &len);

        return _digestBuf;
    }

    IO::IO(const IO::Role role, const std::string& address, const unsigned short portValue, const size_t maxReconnectCnt)
        : addr(address),
          port(static_cast<int>(portValue)),
          _socket(address, portValue),
          _role(role),
          _maxReconnectCnt(maxReconnectCnt) {
        open();
    }

    IO::IO(const bool isServer, const std::string& address, const int portValue, const bool /*quiet*/, const size_t maxReconnectCnt)
        : IO(isServer ? IO::SERVER : IO::CLIENT, address, static_cast<unsigned short>(portValue), maxReconnectCnt) {}

    IO::~IO() {
        try {
            _socket.close();
        } catch (...) {
        }
    }

    bool IO::is_server() const {
        return _role == IO::SERVER;
    }

    void IO::flush() {
        // Boost.Asio socket writes are blocking; nothing buffered here.
    }

    void IO::sync() {
        std::array<std::byte, 1> token {};
        if (is_server()) {
            send_data_internal(token.data(), token.size());
            recv_data_internal(token.data(), token.size());
        } else {
            recv_data_internal(token.data(), token.size());
            send_data_internal(token.data(), token.size());
        }
    }

    void IO::open() {
        if (is_server()) {
            _socket.accept();
        } else {
            _socket.connect(_maxReconnectCnt);
        }
    }

    void IO::send_data_internal(const void* const data, const size_t len) {
        if (len == 0) {
            return;
        }
        _socket.write(data, len);
    }

    void IO::recv_data_internal(void* const data, const size_t len) {
        if (len == 0) {
            return;
        }
        _socket.read(data, len);
    }
}
