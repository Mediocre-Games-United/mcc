#include "logger.hpp"
#include "unit_test.hpp"

using namespace cbu::test;

void cbu::test::test_main(test_result &res,int argc,char **argv) {
    cbu::log_info("Hello from test!");

    res.set_success();
}
