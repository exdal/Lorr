// C++ Standard Library compatibility layer for Slang.
// Not sure about naming of this module, might cause some
// issues in the future.

module std;

// Global namespace
__include std.stdint;

// std namespace
__include std.atomic;
__include std.color;
__include std.encoding;
__include std.math;
