#include <exception>
#include <string>

using namespace std;

class LimitException : public exception {
private:
    string msg;

public:
    explicit LimitException(string str) : msg(std::move(str)) {};

    string get_msg() {
        return msg;
    }
};
