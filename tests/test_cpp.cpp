#include <laststate/latch.hpp>
int main() {
    laststate::Breadcrumb breadcrumb{"cpp_started"};
    { laststate::Span span{42}; }
    laststate::metric("cpp_metric", 1);
    return 0;
}
