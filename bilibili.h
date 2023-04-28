/**
 * bilibili.h
 * 
 * Header file for api of www.bilibili.com
 */

#ifndef _BILIBILI_H_
#define _BILIBILI_H_

#include <string>
#include <vector>

#include "https.h"

class Bilibili {
public:
    explicit Bilibili(const std::string &cookie) : connection(host, cookie) {}
    ~Bilibili() = default;
    void get_index(std::string &recvdata);
private:
    static const std::string host;
    HttpsClient connection;
};

class BiliApi {
public:
    BiliApi(const std::string &cookie);
    ~BiliApi() = default;
    void bullet_chat(const uint32_t roomid, const std::string &msg);
    void sign(std::string &recvdata);
    uint32_t timeStamp();
    void fansMedal(std::vector<uint32_t> &room_id);
    void likeRoom(const uint32_t roomid);
    uint32_t roomPlayInfo(const uint32_t roomid);
    void enterRoom(const uint32_t roomid);
    void heartBeat(const uint32_t roomid);
    void getExp(const uint32_t roomid);
private:
    static const std::string host;
    HttpsClient connection;
    std::string csrf_token;
    std::string anchor_id;
};

#endif /* _BILIBILI_H_ */
