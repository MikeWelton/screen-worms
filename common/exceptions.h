#include <exception>
#include <string>

using namespace std;

class UnknownEventTypeException : public exception {
public:
    UnknownEventTypeException() = default;
};

class IncorrectCrc32Exception : public exception {
public:
    IncorrectCrc32Exception() = default;
};

class LimitException : public exception {
private:
    string msg;

public:
    explicit LimitException(string str) : msg(std::move(str)) {};

    string get_msg() {
        return msg;
    }
};
