## Render Graph submission design

```cpp
struct ImageBarrierHistory
{
	ResourceID m_Resource;	
	Usage m_Usage;
	Access m_Access;
};

...

GraphicsPass *pPass = renderGraph->AddGraphicsPass<PassData>("imgui");
pPass->AddColorAttachment();
```
