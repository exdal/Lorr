# Lorr

[![License](https://img.shields.io/badge/license-AGPL%20v3-blue.svg)](LICENSE)

A toy Vulkan-based renderer for high-performance graphics rendering in lower-end machines. This project aims to provide a flexible and efficient rendering engine with support for advanced features like a render graph, descriptor buffers, descriptor indexing, and more.

## Features

- **Render Graph:** A flexible and extensible render graph system that allows for efficient organization and execution of rendering passes.
- **Descriptor Buffers:** Efficient handling of descriptor buffers to streamline resource management and improve rendering performance.
- **Descriptor Indexing:** Built-in support for descriptor indexing to enhance flexibility and efficiency in managing shader resources.

## Future Plans

- **Global Illumination:** Implementing advanced global illumination techniques to enhance the realism of rendered scenes.
- **Entity Component System (ECS):** Introducing an ECS architecture for improved scalability and maintainability of the codebase.
- **Patch Tracing:** Integrating patch tracing algorithms for more realistic and detailed rendering of surfaces.
- **Interactive Editor UI:** Developing a fully interactive editor UI to empower users with a seamless and intuitive experience.
- **Shader Compilation with HLSL:** Compiling shaders using DXC for improved cross-platform compatibility and development efficiency.
- **Shader Reflection with SPIR-V:** Enabling shader reflection using SPIR-V to provide runtime information about shader resources and parameters.

## Getting Started

1. **Prerequisites:** VulkanSDK, clang(with Ninja), CMake.

2. **Build:** `mkdir build && cmake ./build`

3. **Run Examples:** Examples are built into `bin/example_name-build_type.exe`. Simply run them.

## Contributing

Contributions are always welcomed. To contribute:

1. Fork the repository.
2. Create a new branch for your feature or bug fix.
3. Make your changes and submit a pull request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
