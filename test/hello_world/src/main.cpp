//
// Created by YangHuiwen on 2021/3/7.
//

#include "netp.hpp"
int main(int argc, char** argv) {
    netp::app::instance()->init(argc, argv);
    netp::app::instance()->start_loop();

    NETP_INFO("HELLO WORLD");
    return 0;
}