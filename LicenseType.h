#pragma once

enum LicenseType
{
    LicenseType_Invalid,
    LicenseType_ScreenController,
    LicenseType_Max
};
const char LICENSE_TYPE[LicenseType_Max][64] = {
    "invalid",
    "oosman\\'s screen controller app"
};
