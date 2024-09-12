#pragma once

#define LS_CONCAT_IMPL(x, y) x##y
#define LS_CONCAT(x, y) LS_CONCAT_IMPL(x, y)
#define LS_UNIQUE_VAR() LS_CONCAT(_ls_v_, __COUNTER__)
