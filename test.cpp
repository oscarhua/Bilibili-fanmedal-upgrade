#include <string>
#include <vector>
#include <iostream>
#include <thread>

#include "https.h"
#include "bilibili.h"

int main() {
    HttpsClient::ssl_init();
    std::string recvdata;

    // fill in the cookie here
    std::string bilicookie = "";
    std::string apicookie = "";

    // Support github action
    char *env_bili = getenv("ACTION_BILI");
    if (env_bili != NULL) {
        bilicookie = env_bili;
        apicookie = getenv("ACTION_API");
    }

    Bilibili bili(bilicookie);
    BiliApi biliapi(apicookie);

    biliapi.sign(recvdata);
    std::cout << recvdata << std::endl;

    std::vector<uint32_t> room_id;
    biliapi.fansMedal(room_id);
    for (size_t i = 0; i < room_id.size() - 1; ++i) {
        std::thread(
            [&biliapi] (uint32_t roomid) -> void {
                biliapi.getExp(roomid);
            }
        , room_id[i]).detach();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    biliapi.getExp(room_id.back());

    return 0;
}
