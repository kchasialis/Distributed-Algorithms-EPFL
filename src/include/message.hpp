#pragma once

#include <cstdlib>
#include <functional>

class Message {
private:
    int _seq_id;

public:
    Message(int seq_id);
    int seq_id() const;
    void *data() const;
    std::size_t size() const;
};

struct MessageHash {
    std::size_t operator()(const Message& msg) const {
        return std::hash<int>()(msg.seq_id());
    }
};
