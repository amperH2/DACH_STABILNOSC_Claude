#pragma once
#include <stdint.h>
struct ResetBreadcrumb {
 uint32_t magic,stage,at,endpoint,check;
};
constexpr uint32_t breadcrumbCheck(const ResetBreadcrumb& b) {
 return 0x72c94051u ^ b.magic ^ b.stage ^ b.at ^ b.endpoint;
}
constexpr bool breadcrumbValid(const ResetBreadcrumb& b) {
 return b.magic==0x524f4f46u && b.check==breadcrumbCheck(b);
}
constexpr ResetBreadcrumb breadcrumbRecord(uint32_t stage,uint32_t now,uint32_t endpoint) {
 ResetBreadcrumb b{0x524f4f46u,stage,now,endpoint,0};b.check=breadcrumbCheck(b);return b;
}
