#include "../cornerstone.hxx"
#include <cassert>
#include <fstream>
#include <iostream>

using namespace cornerstone;

void test_logger() {
    asio_service svc;
    std::string text[4];
    text[0] = "line1";
    text[1] = "line2";
    text[2] = "line3";
    text[3] = "line4";
    {
        std::unique_ptr<logger> l(svc.create_logger(asio_service::log_level::debug, "log1.log"));
        l->debug(text[0]);
        l->info(text[1]);
        l->warn(text[2]);
        l->err(text[3]);
    }

    std::ifstream log1("log1.log");
    for (int i = 0; i < 4; ++i) {
        std::string line;
        std::getline(log1, line);
        std::string log_text = line.substr(line.length() - 5);
        assert(log_text == text[i]);
    }

    log1.close();
    {
        std::unique_ptr<logger> l(svc.create_logger(asio_service::log_level::warnning, "log2.log"));
        l->debug(text[0]);
        l->info(text[1]);
        l->warn(text[2]);
        l->err(text[3]);
    }

    std::ifstream log2("log2.log");
    for (int i = 2; i < 4; ++i) {
        std::string line;
        std::getline(log2, line);
        std::string log_text = line.substr(line.length() - 5);
        assert(log_text == text[i]);
    }

    log2.close();

    std::remove("log1.log");
    std::remove("log2.log");
}