# Lorr

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A toy Vulkan-based renderer with bleeding edge features. Works on both Linux and Windows.

## Requirements
If you are willing to help development, make sure you have the latest Vulkan SDK installed.

To build, you would need clang-18 or latest MSVC compiler with proper C++23 support.
Lorr uses xmake build tool, you can install it from [here](https://xmake.io/#/getting_started).

### Notes
- Vulkan SDK still does not support timeline semaphores, most of the validation errors are related to that.
- On Linux, due to my hardware, wayland might not be properly supported and X11 is the main window system.

## Getting Started
To build, simply run `xmake` on project root directory.

You can configure your toolchain using `xmake f --toolchain=clang|cl|gcc --mode=debug|release` before running building.
Make sure to run `xmake install -o ./build` to get third party binaries.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
