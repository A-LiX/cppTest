#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/future-util.hh>
#include <seastar/net/api.hh>
#include <seastar/net/inet_address.hh>
#include <seastar/util/log.hh>
#include <iostream>

using namespace seastar;

future<> seastar_main(const boost::program_options::variables_map& args) {
    auto server_addr = net::inet_address("127.0.0.1");
    uint16_t server_port = 8888;
    auto sa = ipv4_addr(server_addr, server_port);

    return seastar::engine().net().connect(sa).then([](connected_socket fd) {
        auto out = fd.output();
        auto in = fd.input();

        std::string msg = "hello from seastar dpdk client!\n";
        return out.write(msg).then([out = std::move(out), in = std::move(in)]() mutable {
            return out.flush().then([out = std::move(out), in = std::move(in)]() mutable {
                // 读取响应
                return in.read().then([in = std::move(in)](temporary_buffer<char> buf) mutable {
                    if (!buf) {
                        std::cout << "Connection closed by server." << std::endl;
                        return make_ready_future<>();
                    }
                    std::cout << "Received: " << std::string(buf.get(), buf.size()) << std::endl;
                    return make_ready_future<>();
                });
            });
        });
    });
}

int main(int argc, char** argv) {
    app_template app;
    return app.run(argc, argv, [&app] {
        return seastar_main(app.configuration()).then([] {
            return 0;
        });
    });
}