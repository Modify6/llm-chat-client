#include "core/Message.h"

Message Message::user(const std::string& text) {
    return Message{Role::User, text, std::chrono::system_clock::now()};
}

Message Message::assistant(const std::string& text) {
    return Message{Role::Assistant, text, std::chrono::system_clock::now()};
}

Message Message::system(const std::string& text) {
    return Message{Role::System, text, std::chrono::system_clock::now()};
}
