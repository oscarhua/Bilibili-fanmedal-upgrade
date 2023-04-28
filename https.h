/**
 * https.h
 * 
 * Header file for https connection
 */

#ifndef _HTTPS_H_
#define _HTTPS_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

#include <openssl/ssl.h>

enum HttpsMethod { GET, POST };

// structure of the header of https request
typedef std::unordered_map<std::string, std::string> Header;

// structure of https post request body, preserve order
typedef std::vector<std::pair<std::string, std::string>> Form;

// structure of https request
typedef struct {
    HttpsMethod method;
    std::string url;
    Header header;
    // std::string cookie;
} HttpsRequest;


class HttpsClient {
public:
    // initialize ssl, done by user rather than class
    static void ssl_init();

    // header used in each request
    static const Header default_header;

    // given the name of host and cookie, establish an https connection
    explicit HttpsClient(const std::string &host, const std::string &cookie);

    // release ssl and socket resource
    ~HttpsClient();

    // send the request, body if not nullptr, and write the response in recvdata
    void writeread(const HttpsRequest &request, const char *body, std::string &recvdata);
private:

    // the buffer size used to read from socket, 4096 by default
    static const size_t BUFSIZE;

    // establish ssl connection
    void establish();

    // re-establish when we haven't sent message for long time and server closed connection
    void re_establish();

    /**
     * using recvbuf, read len_chunk bytes from the socket and append to recvdata
     * the content to be read is required to end in "\\r\\n", which will be erased by the function
     */
    void read_fixed_size(char *recvbuf, std::string &recvdata, const size_t len_chunk);

    // send all messages using SSL_write
    void write_helper(const std::string &sendbuf);

    // used for synchronization in writeread()
    std::mutex m;

    // store the host
    std::string _host, _cookie;

    // ssl and socket related
    SSL_CTX *ctx;
    SSL *ssl;
    int sockfd;
};

/**
 * Formalize the data using webkitform
 */
class WebkitForm {
public:
    explicit WebkitForm(const Form &form, Header &header);
    ~WebkitForm() = default;
    const char *c_str() { return data.c_str(); }
private:
    // WARNING: USE THE SAME BOUNDARY STRING FOR ALL POST
    static const std::string boundary;
    std::string data;
};

/**
 * Formalize the data using urlencoded
 */
class FormUrlencoded {
public:
    explicit FormUrlencoded(const Form &form, Header &header);
    ~FormUrlencoded() = default;
    const char *c_str() { return data.c_str(); }
private:
    std::string data;
};

#endif /* _HTTPS_H_ */
