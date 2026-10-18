#include <iostream>
#include <string>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

typedef websocketpp::server<websocketpp::config::asio> wsserver_t;

void print(const std::string &str)
{
    std::cout << str << std::endl;
}

// void http_callback(wsserver_t *srv, websocketpp::connection_hdl hdl) {
//     wsserver_t::connection_ptr conn = srv->get_con_from_hdl(hdl);
//     websocketpp::http::parser::request const& req = conn->get_request();

//     std::cout << "method: " << req.get_method() << " uri:" << req.get_uri() << std::endl;

//     // ========== 关键第一步：开启延迟HTTP响应 ==========
//     conn->defer_http_response();

//     std::string body = "<html><body><h1>Hello World</h1></body></html>";
//     conn->set_body(body);
//     conn->append_header("Content-Type", "text/html; charset=utf-8");
//     conn->set_status(websocketpp::http::status_code::ok);

//     // ========== 调用带错误码版本的send_http_response ==========
//     websocketpp::lib::error_code ec;
//     conn->send_http_response(ec);
//     if (ec)
//     {
//         std::cout << "send http response error: " << ec.message() << std::endl;
//     }
// }



void http_callback(wsserver_t *srv, websocketpp::connection_hdl hdl) {
    //给客户端返回一个hello world的页面
    wsserver_t::connection_ptr conn = srv->get_con_from_hdl(hdl);
    std::cout << "body: " << conn->get_request_body() << std::endl; 
    websocketpp::http::parser::request req = conn->get_request();
    std::cout << "method: " << req.get_method() << std::endl;
    std::cout << "uri: " << req.get_uri() << std::endl;

    std::string body = "<html><body><h1>Hello World</h1></body></html>";
    conn->set_body(body);
    conn->append_header("Content-Type", "text/html");
    // conn->set_body(conn->get_request_body());
    // conn->set_status(websocketpp::http::status_code::ok);
    // wsserver_t::timer_ptr tp = srv->set_timer(5000, std::bind(print, "bitejiuyeke"));
    // tp->cancel();//定时任务的取消，会导致定时任务立即被执行
}
void wsopen_callback(wsserver_t *srv, websocketpp::connection_hdl hdl) {
    std::cout << "websocket握手成功！！\n";
}
void wsclose_callback(wsserver_t *srv, websocketpp::connection_hdl hdl) {
    std::cout << "websocket连接断开！！\n";
}
void wsmsg_callback(wsserver_t *srv, websocketpp::connection_hdl hdl, wsserver_t::message_ptr msg) {
    wsserver_t::connection_ptr conn = srv->get_con_from_hdl(hdl);
    std::cout << "wsmsg: " << msg->get_payload() << std::endl;
    std::string rsp = "client say: " + msg->get_payload();
    conn->send(rsp, websocketpp::frame::opcode::text);
}
int main()
{
    //1. 实例化server对象
    wsserver_t wssrv;
    //2. 设置日志等级
    wssrv.set_access_channels(websocketpp::log::alevel::none);
    //3. 初始化asio调度器
    wssrv.init_asio();
    wssrv.set_reuse_addr(true);
    //4. 设置回调函数
    wssrv.set_http_handler(std::bind(http_callback, &wssrv, std::placeholders::_1));
    wssrv.set_open_handler(std::bind(wsopen_callback, &wssrv, std::placeholders::_1));
    wssrv.set_close_handler(std::bind(wsclose_callback, &wssrv, std::placeholders::_1));
    wssrv.set_message_handler(std::bind(wsmsg_callback, &wssrv, std::placeholders::_1, std::placeholders::_2));
    //5. 设置监听端口
    wssrv.listen(8080);
    //6. 开始获取新连接
    wssrv.start_accept();
    //7. 启动服务器
    wssrv.run();
    return 0;
}