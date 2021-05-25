#ifndef SCREEN_WORMS_EXCEPTIONS_H
#define SCREEN_WORMS_EXCEPTIONS_H

#include <exception>
#include <string>

class UnknownEventTypeException : public std::exception {
public:
    UnknownEventTypeException() = default;
};

class IncorrectCrc32Exception : public std::exception {
public:
    IncorrectCrc32Exception() = default;
};

class LimitException : public std::exception {
private:
    std::string msg;

public:
    explicit LimitException(std::string str) : msg(std::move(str)) {};

    std::string get_msg() {
        return msg;
    }
};

#endif //SCREEN_WORMS_EXCEPTIONS_H
