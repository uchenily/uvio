#include "uvio/common/scope_exit.hpp"
#include "uvio/debug.hpp"
#include <sstream>

using namespace uvio;

void basic() {
    LOG_INFO("allocate resource");
    AddScopeExitGuard([]() {
        LOG_INFO("recovery resource");
    });

    LOG_INFO("program running ...");
}

struct HtmlTag {
    void add_tag(std::string_view tag_name) {
        out_ << std::format("<{}>", tag_name);
    }
    void close_tag(std::string_view tag_name) {
        out_ << std::format("</{}>", tag_name);
    }

    void add(std::string_view content) {
        out_ << content;
    }

    void print_html() const {
        std::cout << out_.view();
    }

    std::stringstream out_;
};

void member_func() {
    HtmlTag html_tag;
    {
        html_tag.add_tag("html");
        AddScopeExitGuard([&]() {
            html_tag.close_tag("html");
        });
        {
            html_tag.add_tag("h1");
            AddScopeExitGuard([&]() {
                html_tag.close_tag("h1");
            });
            html_tag.add("This is title");
        }
        {
            html_tag.add_tag("p");
            AddScopeExitGuard([&]() {
                html_tag.close_tag("p");
            });
            html_tag.add(
                "Lorem ipsum dolor sit amet, qui minim labore adipisicing "
                "minim sint cillum sint consectetur cupidatat.");
        }
    }
    html_tag.print_html();
}

auto main() -> int {
    basic();
    member_func();
}
