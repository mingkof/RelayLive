#include "util.h"
#include "ipc.h"
#include "uvIpc.h"
#include <map>

namespace IPC {
    uv_ipc_handle_t* h = NULL;

    
    static std::map<uint32_t, PlayRequest*> _PlayRequests;
    static uint32_t _ID = 0;

    void on_ipc_recv(uv_ipc_handle_t* h, void* user, char* name, char* msg, char* data, int len) {
        if (!strcmp(msg,"live_play_answer")) {
            //播放请求回调 id=123rtpport=80&ret=0&info=XXXX
            data[len] = 0;
            uint32_t id = 0;
            uint32_t port = 0;
            int  ret = 0;
            char szInfo[250] = {0}; // 成功时sdp信息，失败时错误描述
            sscanf(data, "id=%d&port=%d&ret=%d&error=",&id, &port, &ret);

			string info(data, len);
			size_t pos = info.find("error=");
			if(pos != string::npos){
				info = info.substr(pos+6, info.size()-pos-6);
			}

            auto it = _PlayRequests.find(id);
            if(it != _PlayRequests.end()) {
                it->second->port = port;
                it->second->ret = ret;
                it->second->info = info;
                it->second->finish = true;
            }
        } 
    }

    bool Init(int port) {
        /** 进程间通信 */
        char name[20]={0};
        sprintf(name, "livesvr%d", port);
        int ret = uv_ipc_client(&h, "ipcsvr", NULL, name, on_ipc_recv, NULL);
        if(ret < 0) {
            Log::error("ipc server err: %s", uv_ipc_strerr(ret));
            return false;
        }

        return true;
    }

    void Cleanup() {
        uv_ipc_close(h);
    }

    void SendClients(string info) {
        uv_ipc_send(h, "livectrlsvr", "clients", (char*)info.c_str(), info.size());
    }

    PlayRequest RealPlay(std::string code) {
        PlayRequest req;
        req.code   = code;
        req.id     = _ID++;
        req.ret    = 0;
        req.finish = false;
        _PlayRequests.insert(make_pair(req.id, &req));

        time_t send_time = time(NULL);

        std::string msg = "devcode=" + code + "&id=" + to_string(req.id);
        uv_ipc_send(h, "sipsvr", "live_play", (char*)msg.c_str(), msg.size());

        // 等待返回结果
        while (!req.finish) {
            time_t now = time(NULL);
            if(difftime(now, send_time) > 30) {
                req.finish = true;
                req.ret = 0;
                req.info = "time out";
            }
        }

        //返回播放结果
        return req;
    }
}