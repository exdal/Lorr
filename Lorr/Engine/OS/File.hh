#pragma once

namespace lr::os {
enum class FileAccess {
    Write = 1 << 0,
    Read = 1 << 1,
    Execute = 1 << 2,
    SharedWrite = 1 << 3,
    SharedRead = 1 << 4,
};

}
