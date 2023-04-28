#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/err.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <iostream>
#include <errno.h>

#include "https.h"

const size_t HttpsClient::BUFSIZE = 4096;

const Header HttpsClient::default_header = {
    {"Connection", "keep-alive"},
    {"Accept", "*/*"},
    {"User-Agent", "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/111.0.0.0 Safari/537.36"},
    {"Accept-Encoding", "identity"},
    {"Accept-Language", "zh-CN,zh;q=0.9"}
};

void HttpsClient::ssl_init() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
}

HttpsClient::HttpsClient(const std::string &host, const std::string &cookie) : _host(host), _cookie(cookie) {
    const SSL_METHOD *meth = SSLv23_client_method();
    ctx = SSL_CTX_new(meth);
    if(ctx == NULL) {
        throw std::runtime_error("SSL_CTX_new fails");
    }

    ssl = SSL_new(ctx);
    if(ssl == NULL) {
        throw std::runtime_error("SSL_new fails");
    }

    establish();    
}

void HttpsClient::establish() {
    // create a new socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        throw std::runtime_error("Creating new socket fails");
    }
    
    // get ip address
    struct hostent *ip = gethostbyname(_host.c_str());
    if(ip == NULL) {
        throw std::runtime_error("gethostbyname fails");
    }

    struct sockaddr_in addr;
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&(addr.sin_addr), ip->h_addr, ip->h_length);
    addr.sin_port = htons(443);

    // tcp connect
    if(connect(sockfd, (sockaddr *)&addr, sizeof(addr)) == -1) {
        throw std::runtime_error("tcp connect fails");
    }
    
    // ssl connect
    SSL_set_fd(ssl, sockfd);
    SSL_set_connect_state(ssl);
    SSL_set_tlsext_host_name(ssl, _host.c_str());
    if(SSL_connect(ssl) == -1) {
        throw std::runtime_error("ssl connect fails");
    }
}

void HttpsClient::re_establish() {
    SSL_shutdown(ssl);
    close(sockfd);
    establish();
}

HttpsClient::~HttpsClient() {
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);
}

void HttpsClient::read_fixed_size(char *recvbuf, std::string &recvdata, const size_t len_chunk) {
    if (len_chunk == 0) {
        return;
    }
    size_t num_received = 0;
    do {
        // ssl read
        int ret = SSL_read(ssl, recvbuf, static_cast<int>(std::min(BUFSIZE - 1, len_chunk - num_received)));
        if (ret <= 0) {
            throw std::runtime_error("SSL_read fails");
        }
        num_received += ret;

        // append to recvbuf
        recvbuf[ret] = '\0';
        recvdata += std::string(recvbuf);
    } while (num_received < len_chunk);

    // recvdata should end in "\r\n" and this terminator needs to be erased
    if (recvdata.substr(recvdata.length() - 2) != "\r\n") {
        throw std::runtime_error("read_fixed_size not ends in \\r\\n");
    }
    recvdata.erase(recvdata.length() - 2);
}

void HttpsClient::write_helper(const std::string &sendbuf) {
    size_t sent = 0;
    do {
        int ret = SSL_write(ssl, sendbuf.c_str() + sent, static_cast<int>(sendbuf.length() - sent));
        if (ret < 0) {
            throw std::runtime_error("SSL_write fails");
        } else if (ret == 0) {
            // server side closes the connection
            if (sent != 0) {
                throw std::runtime_error("Server closes connection during SSL_write");
            }
            re_establish();
        }
        sent += ret;
    } while (sent < sendbuf.length());
}

void HttpsClient::writeread(const HttpsRequest &request, const char *body, std::string &recvdata) {
    std::string sendbuf;
    
    // request line
    switch (request.method) {
        case HttpsMethod::GET:
            sendbuf = "GET ";
            break;
        case HttpsMethod::POST:
            sendbuf = "POST ";
            break;
    }
    sendbuf += request.url + " HTTP/1.1\r\n";

    // headers
    sendbuf += "Host: " + _host + "\r\n";
    for (auto it : request.header) {
        sendbuf += it.first + ": " + it.second + "\r\n";
    }
    // sendbuf += "Cookie: " + request.cookie + "\r\n\r\n";
    sendbuf += "Cookie: " + _cookie + "\r\n\r\n";

    // message body
    if (body != nullptr) {
        sendbuf += std::string(body);
    }

    // get the lock to send and receive data
    std::lock_guard<std::mutex> lock(m);

    // send data
    write_helper(sendbuf);

    // recvbuf should be null-terminated
    char recvbuf[BUFSIZE];
    // record the position of "\r\n\r\n"
    char *pos_double_crlf;
    size_t num_received = 0;
    // WARNING: ALWAYS ASSUME THAT "\r\n\r\n" EXISTS IN THE FIRST BUFSIZE(4096) BYTES OF RESPONSE
    do {
        int ret = SSL_read(ssl, recvbuf + num_received, static_cast<int>(BUFSIZE - 1 - num_received));
        if (ret < 0) {
            throw std::runtime_error("SSL_read fails");
        } else if (ret == 0) {
            // server side closes the connection
            if (num_received != 0) {
                throw std::runtime_error("Server closes connection during SSL_read");
            }
            re_establish();
            write_helper(sendbuf);
        }
        num_received += ret;
        recvbuf[num_received] = '\0';
        // find the 
        pos_double_crlf = strstr(recvbuf, "\r\n\r\n");
    } while (pos_double_crlf == NULL);

    // DEBUG OUTPUT
    // *pos_double_crlf = '\0';
    // std::cout << recvbuf << std::endl;
    
    // Response code not start with 2 ("HTTP/1.1 " has length 9)
    int response_code;
    if (sscanf(recvbuf + 9ul, "%d", &response_code) != 1 || response_code / 100 != 2) {
        throw std::runtime_error("Status code indicates not success");
    }

    // Return if 204 No Content
    if (response_code == 204) {
        return;
    }

    // Check how the server sends data
    const char *str_cntlen = "Content-Length: ";
    char *pos_cntlen = strstr(recvbuf, str_cntlen);
    if (pos_cntlen == NULL) {
        // Transfer-encoding: chunked
        size_t len_chunk;
        // get the size of the first chunk
        if (sscanf(pos_double_crlf + 4, "%lx", &len_chunk) != 1) {
            throw std::runtime_error("Fail to get the size of first chunk");
        }
        // determine how many bytes remain for the first chunk
        char *data_chunk = strstr(pos_double_crlf + 4, "\r\n");
        if (data_chunk == NULL) {
            throw std::runtime_error("Fail to locate first chunk");
        }
        // skip "\r\n"
        data_chunk += 2;
        recvdata = std::string(data_chunk);

        // only one chunk
        size_t endofchunk = recvdata.find("\r\n0\r\n");
        if (endofchunk != std::string::npos) {
            recvdata.erase(endofchunk);
            return;
        }

        // read the data of chunk include "\r\n"
        read_fixed_size(recvbuf, recvdata, len_chunk + 2 - recvdata.length());
        do {
            num_received = 0;
            do {
                int ret = SSL_read(ssl, recvbuf + num_received, 1);
                if (ret <= 0) {
                    throw std::runtime_error("SSL_read fails");
                }
                num_received += ret;
            } while (recvbuf[num_received - 1] != '\n');
            // read the size of chunk
            if (sscanf(recvbuf, "%lx", &len_chunk) != 1) {
                throw std::runtime_error("Fail to get the size of chunk");
            }
            // read the data of chunk include "\r\n"
            read_fixed_size(recvbuf, recvdata, len_chunk + 2);
        } while (len_chunk != 0);
    } else {
        // Content-Length
        size_t datalen;
        if (sscanf(pos_cntlen + strlen(str_cntlen), "%lu", &datalen) != 1) {
            throw std::runtime_error("Fail to get Content-Length");
        }

        recvdata = std::string(pos_double_crlf + 4ul);
        if (recvdata.length() < datalen) {
            read_fixed_size(recvbuf, recvdata, datalen - recvdata.length());
        }
    }
}

// the same boundary for all post
const std::string WebkitForm::boundary = "iCOaB9gbVqcDvzin";

WebkitForm::WebkitForm(const Form &form, Header &header) : data("") {
    for (auto it : form) {
        data += "------WebKitFormBoundary" + boundary + "\r\n";
        data += "Content-Disposition: form-data; name=\"" + it.first + "\"\r\n\r\n";
        data += it.second + "\r\n";
    }
    data += "------WebKitFormBoundary" + boundary + "--\r\n";
    header.emplace("Content-Length", std::to_string(data.length()));
    header.emplace("Content-Type", "multipart/form-data; boundary=----WebKitFormBoundary" + boundary);
}

FormUrlencoded::FormUrlencoded(const Form &form, Header &header) {
    data = form.front().first + "=" + form.front().second;
    for (size_t i = 1; i < form.size(); ++i) {
        data += "&" + form[i].first + "=" + form[i].second;
    }
    header.emplace("Content-Length", std::to_string(data.length()));
    header.emplace("Content-Type", "application/x-www-form-urlencoded");
}
