#ifndef __M_SS_H__
#define __M_SS_H__

#include "util.hpp"
#include <functional>
#include <mutex>
#include <unordered_map>

typedef enum {UNLOGIN, LOGIN} ss_statu;

class session {
    private:
        // 不可预测的随机凭证；浏览器 Cookie 只保存该值，不暴露 uid。
        std::string _ssid;
        uint64_t _uid;
        ss_statu _statu;
        wsserver_t::timer_ptr _tp;

    public:
        session(const std::string &ssid)
            : _ssid(ssid), _uid(0), _statu(UNLOGIN) {
            DLOG("SESSION %p 被创建！！", this);
        }
        ~session() { DLOG("SESSION %p 被释放！！", this); }

        const std::string& ssid() const { return _ssid; }
        void set_statu(ss_statu statu) { _statu = statu; }
        void set_user(uint64_t uid) { _uid = uid; }
        uint64_t get_user() const { return _uid; }
        bool is_login() const { return _statu == LOGIN; }
        void set_timer(const wsserver_t::timer_ptr &tp) { _tp = tp; }
        wsserver_t::timer_ptr get_timer() const { return _tp; }
};

#define SESSION_TIMEOUT 30000
#define SESSION_FOREVER -1
using session_ptr = std::shared_ptr<session>;

class session_manager {
    private:
        std::mutex _mutex;
        std::unordered_map<std::string, session_ptr> _session;
        wsserver_t *_server;

        // 真正到期时才删除 Session。cancel() 会让定时器回调携带错误码，
        // 此时直接返回，避免旧定时器误删已经续期的 Session。
        void on_session_timeout(const std::string &ssid,
                                const websocketpp::lib::error_code &ec) {
            if (ec) {
                return;
            }
            remove_session(ssid);
        }

    public:
        session_manager(wsserver_t *srv): _server(srv) {
            DLOG("session管理器初始化完毕！");
        }
        ~session_manager() { DLOG("session管理器即将销毁！"); }

        // 生成随机 Session ID；理论上碰撞概率极低，仍在 map 中检查以保证唯一性。
        session_ptr create_session(uint64_t uid, ss_statu statu) {
            for (int attempt = 0; attempt < 5; ++attempt) {
                std::string ssid;
                if (!session_id_util::generate(ssid)) {
                    return session_ptr();
                }

                std::unique_lock<std::mutex> lock(_mutex);
                if (_session.find(ssid) != _session.end()) {
                    continue;
                }

                session_ptr ssp = std::make_shared<session>(ssid);
                ssp->set_statu(statu);
                ssp->set_user(uid);
                _session.insert(std::make_pair(ssid, ssp));
                return ssp;
            }
            ELOG("create session failed: repeated session id collision");
            return session_ptr();
        }

        // 通过 Cookie 中携带的随机 Session ID 获取会话。
        session_ptr get_session_by_ssid(const std::string &ssid) {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _session.find(ssid);
            if (it == _session.end()) {
                return session_ptr();
            }
            return it->second;
        }

        // 退出登录或定时器自然到期时，从内存 Session 表中删除。
        void remove_session(const std::string &ssid) {
            std::unique_lock<std::mutex> lock(_mutex);
            _session.erase(ssid);
        }

        // SESSION_FOREVER 表示取消超时；其他毫秒值表示从现在起重新计时。
        void set_session_expire_time(const std::string &ssid, int ms) {
            session_ptr ssp = get_session_by_ssid(ssid);
            if (ssp.get() == nullptr) {
                return;
            }

            wsserver_t::timer_ptr old_timer = ssp->get_timer();
            if (old_timer.get() != nullptr) {
                // 被取消的回调会得到错误码，on_session_timeout 不会删除 Session。
                old_timer->cancel();
                ssp->set_timer(wsserver_t::timer_ptr());
            }

            if (ms == SESSION_FOREVER) {
                return;
            }

            wsserver_t::timer_ptr new_timer = _server->set_timer(
                ms,
                std::bind(&session_manager::on_session_timeout,
                          this, ssid, std::placeholders::_1));
            ssp->set_timer(new_timer);
        }
};

#endif
