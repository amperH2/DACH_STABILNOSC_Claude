#include "operator_policy.h"
#include "reset_breadcrumb.h"
#include "http_request.h"
#include <type_traits>
using namespace astro_policy;
constexpr bool closeMatrix(){for(int i=0;i<8;++i){bool clearance=i&1,flag=i&2,trip=i&4;if(mountBlocksClose(clearance,flag,trip)!=(trip&&(!clearance||flag)))return false;}return true;}
static_assert(closeMatrix(),"All mount trip/geometry/explicit block combinations");
constexpr bool weatherMatrix(){for(int i=0;i<8;++i){bool fresh=i&1,wind=i&2,sky=i&4;if(weatherMeasurementsReady(fresh,wind,sky)!=(fresh&&wind&&sky))return false;}return true;}
static_assert(weatherMatrix(),"All production weather freshness gates");
static_assert(trustedIPv4("192.168.1.119","192.168.1.119"),"Authorized NINA host");
static_assert(trustedIPv4("192.168.1.119","10.0.0.1,192.168.1.119"),"Multiple explicit hosts");
static_assert(!trustedIPv4("192.168.1.118","192.168.1.119"),"Other host denied");
static_assert(!trustedIPv4("192.168.1.119",""),"Empty list does not authorize");
static_assert(!trustedIPv4("192.168.1.119","192.168.1.119,garbage"),"Malformed tail fails closed");
static_assert(!trustedIPv4("192.168.1.119","192.168.1.119,"),"Empty trailing host fails closed");
constexpr bool ipParsing(){uint32_t v=0;return parseIPv4("192.168.1.119",v)&&v==0xc0a80177u&&!parseIPv4("192.168.1.999",v)&&!parseIPv4("192.168.1.1.evil",v)&&!parseIPv4("192.168.1",v)&&!parseIPv4("1..2.3",v);}
static_assert(ipParsing(),"Exact IPv4 syntax");
static_assert(std::is_trivial<ResetBreadcrumb>::value,"RTC NOINIT must not be initialized by a constructor");
constexpr bool crumb(){auto b=breadcrumbRecord(25,0xffffffffu,2);if(!breadcrumbValid(b))return false;b.stage=26;return !breadcrumbValid(b);}
static_assert(crumb(),"Retained stage checksum and torn-update detection");
constexpr astro_http::Request parse(const char* s){astro_http::Request r{};r.reset(0);for(;*s;++s)r.feed(*s,1);return r;}
constexpr auto origin=parse("PUT /api/v1/dome/0/openshutter HTTP/1.1\r\nHost: roof:11111\r\nOrigin: http://foreign\r\nContent-Length: 0\r\n\r\n");
static_assert(origin.complete()&&astro_http::Request::equal(origin.origin,"http://foreign")&&astro_http::Request::equal(origin.host,"roof:11111"),"Origin and Host survive bounded parser");
static_assert(parse("GET / HTTP/1.1\r\nOrigin: http://a\r\nOrigin: http://b\r\n\r\n").failed(),"Reject duplicate Origin");
static_assert(parse("GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n").failed(),"Reject duplicate Host");
static_assert(sizeof(astro_http::Request)<6000,"Bound each of four HTTP session buffers");
