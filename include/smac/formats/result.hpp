#pragma once
#include <string>
#include <variant>
namespace smac::formats {
struct Error { std::string message; std::size_t offset{}; };
template<class T> using Result=std::variant<T,Error>;
template<class T> bool ok(const Result<T>& r){return std::holds_alternative<T>(r);}
}

