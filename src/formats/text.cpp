#include "smac/formats/text.hpp"
#include <array>
namespace smac::formats {
static void append_utf8(std::string& out,unsigned cp){if(cp<0x80)out.push_back(static_cast<char>(cp));else if(cp<0x800){out.push_back(static_cast<char>(0xC0|(cp>>6)));out.push_back(static_cast<char>(0x80|(cp&63)));}else{out.push_back(static_cast<char>(0xE0|(cp>>12)));out.push_back(static_cast<char>(0x80|((cp>>6)&63)));out.push_back(static_cast<char>(0x80|(cp&63)));}}
std::string cp1252_to_utf8(std::string_view in){static constexpr std::array<unsigned,32> table{0x20AC,0xFFFD,0x201A,0x0192,0x201E,0x2026,0x2020,0x2021,0x02C6,0x2030,0x0160,0x2039,0x0152,0xFFFD,0x017D,0xFFFD,0xFFFD,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x02DC,0x2122,0x0161,0x203A,0x0153,0xFFFD,0x017E,0x0178};std::string out;for(char raw:in){auto c=static_cast<unsigned char>(raw);unsigned cp=c;if(c>=0x80&&c<=0x9F)cp=table[c-0x80];append_utf8(out,cp);}return out;}
std::string normalize_text(std::string_view in){std::string crlf;crlf.reserve(in.size());for(std::size_t i=0;i<in.size();++i){if(in[i]=='\r'){if(i+1<in.size()&&in[i+1]=='\n')continue;crlf.push_back('\n');}else crlf.push_back(in[i]);}return cp1252_to_utf8(crlf);}
}
