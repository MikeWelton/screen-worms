#include "const.h"

using namespace std;

/* Returns result int if conversion is successful or throws exception if result is <= 0
 * or passes exception if stoi threw one. */
int string_to_int(const string &str) {
    int result = stoi(str);
    if (result <= 0) {
        throw exception();
    }
    return result;
}

bool player_name_valid(const string &name) {
    const int MAX_LEN = 20,
            ASCII_LOWER_BOUND = 33,
            ASCII_UPPER_BOUND = 126;

    bool correct_len = (name.length() <= MAX_LEN);

    if (!correct_len) {
        return false;
    }

    return all_of(name.begin(), name.end(), [](char c) {
        return (ASCII_LOWER_BOUND <= c && c <= ASCII_UPPER_BOUND);
    });
}

uint32_t calculate_checksum(char *data, uint32_t size) {
    uint32_t ret = 0xFFFFFFFF;
    for (int i = 0; i < size; ++i) {

    }
}

// TODO
string resolve_host(const string& addr);
/*string resolve_host(const string& addr) {
    struct addrinfo hints, *res, *result;
    int errcode;
    char addrstr[100];
    void *ptr;

    memset (&hints, 0, sizeof (hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;

    errcode = getaddrinfo (host, NULL, &hints, &result);
    if (errcode != 0)
    {
        perror ("getaddrinfo");
        return -1;
    }

    res = result;

    printf ("Host: %s\n", host);
    while (res)
    {
        inet_ntop (res->ai_family, res->ai_addr->sa_data, addrstr, 100);

        switch (res->ai_family)
        {
            case AF_INET:
                ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
                break;
            case AF_INET6:
                ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
                break;
        }
        inet_ntop (res->ai_family, ptr, addrstr, 100);
        printf ("IPv%d address: %s (%s)\n", res->ai_family == PF_INET6 ? 6 : 4,
                addrstr, res->ai_canonname);
        res = res->ai_next;
    }

    freeaddrinfo(result);
}*/

void exit_error(const string &msg) {
    cout << msg << endl;
    exit(EXIT_FAILURE);
}

