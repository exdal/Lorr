## Render Graph submission design

### Design 1:
```cpp
renderGraph->AddGraphicsPass<PassData>("imgui")
	->SetAttachment("backbuffer", WRITE | READ) // loadop, storeop
	->SetAttachment("imgui_depth", WRITE)
	->SetInputLayout({ float4, float2, uint })
	->SetPushConstant({ 0, offset, size })
	->SetShader(VERTEX, ...)
	->SetShader(PIXEL, ...)
	->SetDescriptorLayout();
	->SetCallbacks(
		[](...) // Setup -- Resource creation
		{
		},
		[](...) // Prepare (Optional) -- Resource data modification
		{
		},
		[](...) // Execute -- Pass execution
		{
		},
		[](...) // Cleanup -- Resource deletion
		{
		});
```

### Design 2:
```cpp
renderGraph->AddGraphicsPass<PassData>(
	"imgui",
	[](RenderPass &pass) // Initialize -- Initialization of pass
	{
		pass.AddAttachment("backbuffer", WRITE | READ) // loadop, storeop
		pass.AddAttachment("imgui_depth", WRITE)
		pass.SetInputLayout({ float4, float2, uint })
		pass.AddPushConstant({ 0, offset, size })
		pass.AddShader(VERTEX, ...)
		pass.AddShader(PIXEL, ...)
	},
	[](...) // Setup -- Resource creation
	{
	},
	[](...) // Prepare (Optional) -- Resource data modification
	{
	},
	[](...) // Execute -- Pass execution
	{
	},
	[](...) // Cleanup -- Resource deletion
	{
	});
```