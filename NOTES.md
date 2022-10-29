# Issues
- Fixed. Deleting a temporary staging buffer gives a validation error because it's being processed in GPU while we delete it. We should wait for that command list to execute. Modify `ExecuteCommandList` function to put a wait operation.

# Pipeline
- Pipeline abstraction is designed as it was D3D12's pipeline because of Vulkan's Pipeline is larger and doesn't take many stuff as dynamic, we set elements that doesn't as dynamic state.

- Due to differences between D3D12 and Vulkan, Vulkan Pipeline has some of it's element set as dynamic to match the D3D12 Pipeline. Tessellation patch count for Vulkan is also set to dynamic. The problem is that it's not supported by VKCore, and it's enabled by extension.

- ### D3D12 Pipeline
- Unlike Vulkan, D3D12's Pipeline Layout is packed into Root Signature description. 
- For regular descriptor sets it's similar to VKAPI implementation. We don't set any space(vk's set) when we create them. But when we create D3D12 Pipeline, we also create a seperate root signature that contains those descriptor sets we created. Each Descriptor Set has it's own space. We define it by creating 'Descriptor Tables' for each of them.