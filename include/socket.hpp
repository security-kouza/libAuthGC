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

#ifndef ATLab_SOCKET
#define ATLab_SOCKET

#include <cstddef>
#include <array>
#include <string>
#include <boost/asio.hpp>
#include <openssl/evp.h>

namespace ATLab {
    class Socket {
        const boost::asio::ip::tcp::endpoint _endpoint;
        boost::asio::io_context _ioc;
        boost::asio::ip::tcp::socket _socket;

        // for SHA256
        EVP_MD_CTX* const _mdctx;
        static constexpr size_t DIGEST_SIZE {32};
        static const EVP_MD* DIGEST_TYPE;
        std::array<std::byte, DIGEST_SIZE> _digestBuf;

    public:
        Socket() = delete;
        Socket(Socket&) = delete;
        Socket(const std::string& address, unsigned short port);

        ~Socket() {
            EVP_MD_CTX_free(_mdctx);
        }

        void accept();
        void connect(size_t MaxReconnectCnt = 5);
        void close();

        size_t read(void* pBuf, size_t size, const std::string& logMsg = "", bool debug = false);
        size_t write(const void* pBuf, size_t size, const std::string& logMsg = "", bool debug = false);

        std::array<std::byte, DIGEST_SIZE> gen_challenge();
    };
}

#endif // ATLab_SOCKET
