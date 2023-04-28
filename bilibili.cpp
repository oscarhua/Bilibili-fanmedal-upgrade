#include <iostream>
#include <thread>

#include "bilibili.h"

size_t read_json(const std::string &recvdata, const std::string &name, size_t pos, uint32_t &value) {
    pos = recvdata.find(name, pos);
    if (pos == std::string::npos) {
        return pos;
    }
    pos += name.length();
    value = 0;
    while (recvdata[pos] != ',') {
        value = value * 10 + recvdata[pos] - '0';
        ++pos;
    }
    return pos;
}

const std::string Bilibili::host = "www.bilibili.com";

void Bilibili::get_index(std::string &recvdata) {
    HttpsRequest req;
    req.url = "/";
    req.method = HttpsMethod::GET;
    req.header = HttpsClient::default_header;
    connection.writeread(req, nullptr, recvdata);
}

const std::string BiliApi::host = "api.live.bilibili.com";

BiliApi::BiliApi(const std::string &cookie) : connection(host, cookie) {
    const std::string str_bili_jct = "bili_jct=";
    const std::string str_DedeUserID = "DedeUserID=";

    size_t pos = cookie.find(str_bili_jct);
    if (pos == std::string::npos) {
        throw std::runtime_error("bili_jct not found in cookie");
    }
    pos += str_bili_jct.length();
    size_t end = cookie.find(';', pos);
    csrf_token = cookie.substr(pos, end - pos);

    pos = cookie.find(str_DedeUserID);
    if (pos == std::string::npos) {
        throw std::runtime_error("DedeUserID not found in cookie");
    }
    pos += str_DedeUserID.length();
    end = cookie.find(';', pos);
    anchor_id = cookie.substr(pos, end - pos);
}

void BiliApi::bullet_chat(const uint32_t roomid, const std::string &msg) {
    std::string recvdata;
    HttpsRequest req;
    req.url = "/msg/send";
    req.method = HttpsMethod::POST;
    req.header = HttpsClient::default_header;

    Form form = {
        {"bubble", "0"},
        {"msg", msg},
        {"color", "16777215"},
        {"mode", "1"},
        {"fontsize", "25"},
        {"rnd", "1681331507"},
        {"roomid", std::to_string(roomid)},
        {"csrf", csrf_token},
        {"csrf_token", csrf_token}
    };
    WebkitForm data(form, req.header);

    req.header.emplace("Origin", "https://live.bilibili.com");
    req.header.emplace("Referer", "https://live.bilibili.com/" + std::to_string(roomid) + "/");
    connection.writeread(req, data.c_str(), recvdata);
}

void BiliApi::sign(std::string &recvdata) {
    HttpsRequest req;
    req.url = "/xlive/web-ucenter/v1/sign/DoSign";
    req.method = HttpsMethod::GET;
    req.header = HttpsClient::default_header;
    connection.writeread(req, nullptr, recvdata);
}

uint32_t BiliApi::timeStamp() {
    std::string recvdata;
    HttpsRequest req;
    req.url = "/xlive/open-interface/v1/rtc/getTimestamp";
    req.method = HttpsMethod::GET;
    req.header = HttpsClient::default_header;
    connection.writeread(req, nullptr, recvdata);

    const std::string str_timestamp = "\"timestamp\":";
    uint32_t ret = 0;
    read_json(recvdata, str_timestamp, 0, ret);
    return ret;
}

void BiliApi::fansMedal(std::vector<uint32_t> &room_id) {
    std::string recvdata;
    HttpsRequest req;
    // change page_size if the total number is greater than 30
    req.url = "/xlive/app-ucenter/v1/fansMedal/panel?page=1&page_size=30";
    req.method = HttpsMethod::GET;
    req.header = HttpsClient::default_header;
    connection.writeread(req, nullptr, recvdata);

    const std::string str_roomid = "\"room_id\":";
    uint32_t id = 0;
    size_t pos = read_json(recvdata, str_roomid, 0, id);
    while (pos != std::string::npos) {
        room_id.push_back(id);
        pos = read_json(recvdata, str_roomid, pos, id);
    }
}

void BiliApi::likeRoom(const uint32_t roomid) {
    std::string recvdata;
    HttpsRequest req;
    req.url = "/xlive/app-ucenter/v1/like_info_v3/like/likeReportV3";
    req.method = HttpsMethod::POST;
    req.header = HttpsClient::default_header;

    Form form = {
        {"room_id", std::to_string(roomid)},
        {"anchor_id", anchor_id},
        {"ts", std::to_string(timeStamp())},
        {"csrf", csrf_token},
        {"csrf_token", csrf_token},
        {"visit_id", ""}
    };
    FormUrlencoded data(form, req.header);

    connection.writeread(req, data.c_str(), recvdata);
}

uint32_t BiliApi::roomPlayInfo(const uint32_t roomid) {
    std::string recvdata;
    HttpsRequest req;
    req.url = "/xlive/web-room/v2/index/getRoomPlayInfo?room_id=" + std::to_string(roomid);
    req.method = HttpsMethod::GET;
    req.header = HttpsClient::default_header;
    connection.writeread(req, nullptr, recvdata);

    const std::string str_live_status = "\"live_status\":";
    uint32_t ret = 0;
    read_json(recvdata, str_live_status, 0, ret);
    return ret;
}

void BiliApi::enterRoom(const uint32_t roomid) {
    std::string recvdata;
    HttpsRequest req;
    req.url = "/xlive/web-room/v1/index/roomEntryAction";
    req.method = HttpsMethod::POST;
    req.header = HttpsClient::default_header;

    Form form = {
        {"room_id", std::to_string(roomid)},
        {"platform", "pc"},
        {"csrf_token", csrf_token},
        {"csrf", csrf_token},
        {"visit_id", ""}
    };
    FormUrlencoded data(form, req.header);

    connection.writeread(req, data.c_str(), recvdata);
}

void BiliApi::heartBeat(const uint32_t room_id) {
    std::string recvdata;
    HttpsRequest req;
    req.url = "/relation/v1/Feed/heartBeat";
    req.method = HttpsMethod::GET;
    req.header = HttpsClient::default_header;
    req.header.emplace("Origin", "https://www.bilibili.com");
    req.header.emplace("Referer", "https://live.bilibili.com/" + std::to_string(room_id));
    connection.writeread(req, nullptr, recvdata);
}

void BiliApi::getExp(const uint32_t roomid) {
    bullet_chat(roomid, "1");
    likeRoom(roomid);
    std::cout << "Room id = " << roomid << " bullet chat and like sent" << std::endl;
    bool live = false;
    while (true) {
        uint32_t status = roomPlayInfo(roomid);
        if (live) {
            if (status == 1) {
                heartBeat(roomid);
                std::this_thread::sleep_for(std::chrono::seconds(30));
            } else {
                live = false;
                std::cout << "Room id = " << roomid << " ends the stream." << std::endl;
            }
        } else if (status == 1) {
            std::cout << "Room id = " << roomid << " starts the stream." << std::endl;
            enterRoom(roomid);
            live = true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}
