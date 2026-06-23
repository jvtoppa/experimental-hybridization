#ifndef UTILS_H
#define UTILS_H

#include <cmath>
#include <iostream>
#include <span>
#include <expected>
#include <numeric>
#include <vector>

using namespace std;

template<typename T> std::ostream& operator<<(std::ostream& out, const std::vector<T>& v) {
    out << '[';
    for (size_t i = 0; i < v.size(); ++i)
    {
        out << v[i];
        if (i != v.size() - 1)
        {
            out << ", "; 
        }
    }
    out << ']';
    return out;
}

#endif // UTILS_H