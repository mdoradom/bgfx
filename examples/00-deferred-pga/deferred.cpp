#include <bx/uint32_t.h>
#include "common.h"
#include "bgfx_utils.h"
#include "logo.h"
#include "imgui/imgui.h"

namespace
{

class Deferred : public entry::AppI
{
public:
	Deferred(const char* _name, const char* _description, const char* _url)
		: entry::AppI(_name, _description, _url)
		, m_supportedBackend(false)
		, m_enableShadows(true)
		, m_enableIBL(true)
		, m_showGBuffer(false)
		, m_showShadowMap(false)
		, m_shadowResolution(2048)
		, m_shadowBias(0.0015f)
		, m_shadowNormalBias(0.02f)
		, m_directionalIntensity(4.0f)
		, m_ambientIblIntensity(1.0f)
	{
	}

	void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
	{
		Args args(_argc, _argv);

		m_width  = _width;
		m_height = _height;
		m_debug  = BGFX_DEBUG_TEXT;
		m_reset  = BGFX_RESET_VSYNC;

		bgfx::Init init;
		init.type     = args.m_type;
		init.vendorId = args.m_pciId;
		init.platformData.nwh  = entry::getNativeWindowHandle(entry::kDefaultWindowHandle);
		init.platformData.ndt  = entry::getNativeDisplayHandle();
		init.platformData.type = entry::getNativeWindowHandleType();
		init.resolution.width  = m_width;
		init.resolution.height = m_height;
		init.resolution.reset  = m_reset;
		bgfx::init(init);

		const bgfx::Caps* caps = bgfx::getCaps();
		m_supportedBackend = caps->rendererType == bgfx::RendererType::OpenGL
			|| caps->rendererType == bgfx::RendererType::Vulkan;

		// Enable debug text.
		bgfx::setDebug(m_debug);

		// Set view 0 clear state.
		bgfx::setViewClear(0
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0x303030ff
			, 1.0f
			, 0
			);

		imguiCreate();
	}

	virtual int shutdown() override
	{
		imguiDestroy();

		// Shutdown bgfx.
		bgfx::shutdown();

		return 0;
	}

	bool update() override
	{
		if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState) )
		{
			imguiBeginFrame(m_mouseState.m_mx
				,  m_mouseState.m_my
				, (m_mouseState.m_buttons[entry::MouseButton::Left  ] ? IMGUI_MBUT_LEFT   : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Right ] ? IMGUI_MBUT_RIGHT  : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
				,  m_mouseState.m_mz
				, uint16_t(m_width)
				, uint16_t(m_height)
				);

			showExampleDialog(this);
			ImGui::SetNextWindowPos(ImVec2(10.0f, 100.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(360.0f, 290.0f), ImGuiCond_FirstUseEver);
			ImGui::Begin("Deferred Debug", NULL, 0);
			ImGui::Text("Backend support: %s", m_supportedBackend ? "OpenGL/Vulkan path enabled" : "Fallback mode");
			ImGui::Checkbox("Enable Shadows", &m_enableShadows);
			ImGui::Checkbox("Enable IBL", &m_enableIBL);
			ImGui::Checkbox("Show GBuffer", &m_showGBuffer);
			ImGui::Checkbox("Show Shadow Map", &m_showShadowMap);
			const char* shadowResItems[] = { "1024", "2048", "4096" };
			int32_t shadowResIndex = m_shadowResolution == 1024 ? 0 : (m_shadowResolution == 4096 ? 2 : 1);
			if (ImGui::Combo("Shadow Resolution", &shadowResIndex, shadowResItems, BX_COUNTOF(shadowResItems) ) )
			{
				m_shadowResolution = shadowResIndex == 0 ? 1024 : (shadowResIndex == 2 ? 4096 : 2048);
			}
			ImGui::SliderFloat("Shadow Bias", &m_shadowBias, 0.0001f, 0.01f, "%.4f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("Normal Bias", &m_shadowNormalBias, 0.0f, 0.2f, "%.4f");
			ImGui::SliderFloat("Directional Intensity", &m_directionalIntensity, 0.0f, 10.0f, "%.2f");
			ImGui::SliderFloat("Ambient IBL Intensity", &m_ambientIblIntensity, 0.0f, 5.0f, "%.2f");
			ImGui::End();

			imguiEndFrame();

			// Set view 0 default viewport.
			bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height) );

			// This dummy draw call is here to make sure that view 0 is cleared
			// if no other draw calls are submitted to view 0.
			bgfx::touch(0);

			// Use debug font to print information about this example.
			bgfx::dbgTextClear();
			const bgfx::Stats* stats = bgfx::getStats();

			if (!m_supportedBackend)
			{
				bgfx::dbgTextPrintf(0, 1, 0x4f, "Deferred path fallback: backend not supported yet.");
				bgfx::dbgTextPrintf(0, 2, 0x0f, "Use OpenGL or Vulkan to enable deferred + shadow map + IBL.");
			}
			else
			{
				bgfx::dbgTextPrintf(0, 1, 0x2f, "Deferred WIP path active (OpenGL/Vulkan).");
			}

			bgfx::dbgTextImage(
				  bx::max<uint16_t>(uint16_t(stats->textWidth/2), 20)-20
				, bx::max<uint16_t>(uint16_t(stats->textHeight/2),  6)-6
				, 40
				, 12
				, s_logo
				, 160
				);

			bgfx::dbgTextPrintf(0, 3, 0x0f, "Backbuffer %dW x %dH in pixels, debug text %dW x %dH in characters."
				, stats->width
				, stats->height
				, stats->textWidth
				, stats->textHeight
				);

			// Advance to next frame. Rendering thread will be kicked to
			// process submitted rendering primitives.
			bgfx::frame();

			return true;
		}

		return false;
	}

	entry::MouseState m_mouseState;

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_debug;
	uint32_t m_reset;

	// Deferred debug options.
	bool m_supportedBackend;
	bool m_enableShadows;
	bool m_enableIBL;
	bool m_showGBuffer;
	bool m_showShadowMap;
	uint16_t m_shadowResolution;
	float m_shadowBias;
	float m_shadowNormalBias;
	float m_directionalIntensity;
	float m_ambientIblIntensity;
};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	  Deferred
	, "00-deferred-pga"
	, "Deferred renderer (WIP): directional + ambient IBL + shadow map."
	, "https://bkaradzic.github.io/bgfx/examples.html#deferred-pga"
	);
