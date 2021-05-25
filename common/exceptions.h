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

class IncorrectNumberException : public std::exception {
private:
    char c;
public:
    IncorrectNumberException(char _c) : c(_c) {};

    const char *what() {
        std::string str = "Incorrect number character " + std::to_string(c);
        const char *ret = str.c_str();
        return ret;
    }
};

class LimitException : public std::exception {
private:
    std::string msg;

public:
    explicit LimitException(std::string str) : msg(std::move(str)) {};

    const char *what() {
        return msg.c_str();
    }
};

#endif //SCREEN_WORMS_EXCEPTIONS_H
